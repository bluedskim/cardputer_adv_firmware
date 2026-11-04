package com.cardputer.companion

import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test
import java.security.KeyPairGenerator

class SshTofuHostKeyLogicTest {

    private val pinA = SshHostKeyPin("ssh-ed25519", "a".repeat(64))
    private val pinB = SshHostKeyPin("ssh-ed25519", "b".repeat(64))

    @Test
    fun firstSeenNeedsTrustPrompt() {
        assertEquals(
            SshTofuHostKeyLogic.Outcome.NEED_TRUST_PROMPT,
            SshTofuHostKeyLogic.evaluate(null, pinA)
        )
    }

    @Test
    fun matchingPinAllowsSilent() {
        assertEquals(
            SshTofuHostKeyLogic.Outcome.ALLOW_MATCH,
            SshTofuHostKeyLogic.evaluate(pinA, pinA.copy(sha256Hex = pinA.sha256Hex.uppercase()))
        )
    }

    @Test
    fun mismatchRejects() {
        assertEquals(
            SshTofuHostKeyLogic.Outcome.REJECT_MISMATCH,
            SshTofuHostKeyLogic.evaluate(pinA, pinB)
        )
    }

    @Test
    fun afterForgetBehavesLikeFirstSeen() {
        val store = InMemoryHostKeyStore()
        store.putPin("example.com", 22, pinA)
        store.remove("example.com", 22)
        assertNull(store.getPin("example.com", 22))
        assertEquals(
            SshTofuHostKeyLogic.Outcome.NEED_TRUST_PROMPT,
            SshTofuHostKeyLogic.evaluate(store.getPin("example.com", 22), pinB)
        )
    }
}

class SshHostKeyPinStorageTest {

    @Test
    fun parsesPipeFormat() {
        val hex = "ab".repeat(32)
        val pin = SshHostKeyPin.fromStorageString("ssh-rsa|$hex")
        assertNotNull(pin)
        assertEquals("ssh-rsa", pin!!.keyType)
    }

    @Test
    fun parsesLegacyHexOnly() {
        val hex = "c".repeat(64)
        val pin = SshHostKeyPin.fromStorageString(hex)
        assertNotNull(pin)
        assertEquals("ssh-unknown", pin!!.keyType)
        assertEquals(hex, pin.sha256Hex)
    }

    @Test
    fun roundTripStorage() {
        val original = SshHostKeyPin("ssh-ed25519", "d".repeat(64))
        val parsed = SshHostKeyPin.fromStorageString(original.toStorageString())
        assertEquals(original, parsed)
    }

    @Test
    fun parsesJsonLegacy() {
        val hex = "fe".repeat(32)
        val pin = SshHostKeyPin.fromStorageString("""{"type":"ssh-ecdsa","sha256":"$hex"}""")
        assertNotNull(pin)
        assertEquals("ssh-ecdsa", pin!!.keyType)
    }
}

class InteractiveTofuHostKeyVerifierTest {

    @Test
    fun pinsOnlyAfterUserAccepts() {
        val store = InMemoryHostKeyStore()
        var prompted = false
        val prompter = object : SshHostKeyTrustPrompter {
            override fun confirmNewHost(host: String, port: Int, offered: SshHostKeyPin): Boolean {
                prompted = true
                return true
            }

            override fun onHostKeyMismatch(
                host: String,
                port: Int,
                stored: SshHostKeyPin,
                offered: SshHostKeyPin
            ) {
            }
        }
        val verifier = InteractiveTofuHostKeyVerifier("host", 22, store, prompter) { }
        val key = testSshPublicKey()
        assertTrue(verifier.verify("host", 22, key))
        assertTrue(prompted)
        assertNotNull(store.getPin("host", 22))
    }

    @Test
    fun declineDoesNotPin() {
        val store = InMemoryHostKeyStore()
        val prompter = object : SshHostKeyTrustPrompter {
            override fun confirmNewHost(host: String, port: Int, offered: SshHostKeyPin) = false
            override fun onHostKeyMismatch(
                host: String,
                port: Int,
                stored: SshHostKeyPin,
                offered: SshHostKeyPin
            ) {
            }
        }
        val verifier = InteractiveTofuHostKeyVerifier("host", 22, store, prompter) { }
        assertFalse(verifier.verify("host", 22, testSshPublicKey()))
        assertNull(store.getPin("host", 22))
    }

    @Test
    fun mismatchBlocksAndNotifies() {
        val store = InMemoryHostKeyStore()
        store.putPin("host", 22, SshHostKeyPin("ssh-ed25519", "e".repeat(64)))
        var mismatch = false
        val prompter = object : SshHostKeyTrustPrompter {
            override fun confirmNewHost(host: String, port: Int, offered: SshHostKeyPin) = true
            override fun onHostKeyMismatch(
                host: String,
                port: Int,
                stored: SshHostKeyPin,
                offered: SshHostKeyPin
            ) {
                mismatch = true
            }
        }
        val verifier = InteractiveTofuHostKeyVerifier("host", 22, store, prompter) { }
        assertFalse(verifier.verify("host", 22, testSshPublicKey()))
        assertTrue(mismatch)
    }
}

class InMemoryHostKeyStore : SshHostKeyRepository {
    private val map = mutableMapOf<String, String>()

    override fun getPin(host: String, port: Int): SshHostKeyPin? =
        map[SshHostKeyStore.storageKey(host, port)]?.let { SshHostKeyPin.fromStorageString(it) }

    override fun putPin(host: String, port: Int, pin: SshHostKeyPin) {
        map[SshHostKeyStore.storageKey(host, port)] = pin.toStorageString()
    }

    override fun remove(host: String, port: Int) {
        map.remove(SshHostKeyStore.storageKey(host, port))
    }
}

private fun testSshPublicKey(): java.security.PublicKey {
    val kpg = java.security.KeyPairGenerator.getInstance("RSA")
    kpg.initialize(512)
    return kpg.generateKeyPair().public
}
