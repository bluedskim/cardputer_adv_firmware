package com.cardputer.companion

import net.schmizz.sshj.common.SecurityUtils
import org.bouncycastle.jce.provider.BouncyCastleProvider
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNotNull
import org.junit.Assert.assertTrue
import org.junit.BeforeClass
import org.junit.Test
import java.security.Security

class SshAuthMethodTest {

    @Test
    fun fromStoredDefaultsToPassword() {
        assertEquals(SshAuthMethod.PASSWORD, SshAuthMethod.fromStored(null))
        assertEquals(SshAuthMethod.PASSWORD, SshAuthMethod.fromStored(""))
        assertEquals(SshAuthMethod.PASSWORD, SshAuthMethod.fromStored("password"))
    }

    @Test
    fun fromStoredRecognizesPrivateKey() {
        assertEquals(SshAuthMethod.PRIVATE_KEY, SshAuthMethod.fromStored("private_key"))
        assertEquals(SshAuthMethod.PRIVATE_KEY, SshAuthMethod.fromStored("key"))
    }

    @Test
    fun roundTripStoredValue() {
        assertEquals("password", SshAuthMethod.toStored(SshAuthMethod.PASSWORD))
        assertEquals("private_key", SshAuthMethod.toStored(SshAuthMethod.PRIVATE_KEY))
    }
}

class SshKeyLoaderTest {

    companion object {
        @BeforeClass
        @JvmStatic
        fun installBouncyCastle() {
            val existing = Security.getProvider(BouncyCastleProvider.PROVIDER_NAME)
            if (existing != null) {
                Security.removeProvider(BouncyCastleProvider.PROVIDER_NAME)
            }
            Security.insertProviderAt(BouncyCastleProvider(), 1)
            SecurityUtils.setRegisterBouncyCastle(true)
        }

        private fun loadResource(name: String): String {
            val stream = checkNotNull(javaClass.classLoader?.getResourceAsStream(name)) {
                "missing test resource $name"
            }
            return stream.bufferedReader().readText()
                .lines()
                .filterNot { it.startsWith("#") }
                .joinToString("\n")
                .trim()
        }
    }

    @Test
    fun loadsUnencryptedOpenSshRsa() {
        val pem = loadResource("test-only-rsa-openssh.key")
        val provider = SshKeyLoader.loadKeyProvider(pem, "")
        assertNotNull(provider.public)
        assertTrue(pem.contains("OPENSSH PRIVATE KEY"))
    }

    @Test
    fun loadsEncryptedEd25519WithPassphrase() {
        val pem = loadResource("test-only-ed25519.key")
        val provider = SshKeyLoader.loadKeyProvider(pem, "testpass")
        assertNotNull(provider.public)
    }

    @Test
    fun wrongPassphraseThrowsClearMessage() {
        val pem = loadResource("test-only-ed25519.key")
        try {
            SshKeyLoader.loadKeyProvider(pem, "wrong")
            throw AssertionError("expected ParseException")
        } catch (e: SshKeyLoader.ParseException) {
            assertEquals("Wrong key passphrase", e.message)
        }
    }

    @Test
    fun emptyKeyRejected() {
        try {
            SshKeyLoader.loadKeyProvider("  ", "")
            throw AssertionError("expected ParseException")
        } catch (e: SshKeyLoader.ParseException) {
            assertEquals("Private key is empty", e.message)
        }
    }
}
