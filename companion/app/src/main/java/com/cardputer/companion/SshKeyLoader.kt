package com.cardputer.companion

import net.schmizz.sshj.userauth.keyprovider.FileKeyProvider
import net.schmizz.sshj.userauth.keyprovider.KeyFormat
import net.schmizz.sshj.userauth.keyprovider.KeyProvider
import net.schmizz.sshj.userauth.keyprovider.KeyProviderUtil
import net.schmizz.sshj.userauth.keyprovider.OpenSSHKeyFile
import net.schmizz.sshj.userauth.keyprovider.PKCS8KeyFile
import net.schmizz.sshj.userauth.password.PasswordFinder
import net.schmizz.sshj.userauth.password.Resource
import com.hierynomus.sshj.userauth.keyprovider.OpenSSHKeyV1KeyFile
import java.io.StringReader

/**
 * Parses PEM / OpenSSH private key text into an sshj [KeyProvider].
 * Does not log key material.
 */
object SshKeyLoader {

    class ParseException(message: String, cause: Throwable? = null) : Exception(message, cause)

    fun loadKeyProvider(pem: String, passphrase: String): KeyProvider {
        val trimmed = pem.trim()
        if (trimmed.isEmpty()) {
            throw ParseException("Private key is empty")
        }
        if (!trimmed.contains("PRIVATE KEY")) {
            throw ParseException("Not a PEM or OpenSSH private key")
        }
        val passChars = passphrase.toCharArray()
        val finder = object : PasswordFinder {
            override fun reqPassword(resource: Resource<*>?): CharArray? =
                if (passChars.isEmpty()) null else passChars.clone()

            override fun shouldRetry(resource: Resource<*>?): Boolean = false
        }
        val format = try {
            KeyProviderUtil.detectKeyFileFormat(StringReader(trimmed), false)
        } catch (e: Exception) {
            throw ParseException(
                e.message?.takeIf { it.isNotBlank() } ?: "Could not detect key format",
                e
            )
        }
        val provider: FileKeyProvider = when (format) {
            KeyFormat.OpenSSH, KeyFormat.OpenSSHv1 -> {
                if (format == KeyFormat.OpenSSHv1 || trimmed.contains("BEGIN OPENSSH PRIVATE KEY")) {
                    OpenSSHKeyV1KeyFile()
                } else {
                    OpenSSHKeyFile()
                }
            }
            KeyFormat.PKCS8 -> PKCS8KeyFile()
            KeyFormat.PuTTY -> throw ParseException("PuTTY .ppk keys are not supported; convert to OpenSSH format")
            KeyFormat.Unknown -> throw ParseException("Unrecognized private key format")
        }
        try {
            provider.init(StringReader(trimmed), finder)
            provider.public
        } catch (e: Exception) {
            throw mapInitFailure(e)
        }
        return provider
    }

    fun keyTypeLabel(pem: String): String {
        val t = pem.trim()
        return when {
            t.contains("BEGIN OPENSSH PRIVATE KEY") && t.contains("ed25519") -> "ed25519 (OpenSSH)"
            t.contains("BEGIN OPENSSH PRIVATE KEY") -> "OpenSSH format"
            t.contains("RSA PRIVATE KEY") -> "RSA (PEM)"
            t.contains("EC PRIVATE KEY") -> "ECDSA (PEM)"
            t.contains("BEGIN PRIVATE KEY") -> "PKCS#8"
            else -> "private key"
        }
    }

    private fun mapInitFailure(e: Exception): ParseException {
        val msg = e.message.orEmpty().lowercase()
        return when {
            msg.contains("bad password") ||
                msg.contains("incorrect passphrase") ||
                msg.contains("decrypt") ||
                msg.contains("bad decrypt") ||
                (msg.contains("password") && msg.contains("fail")) ->
                ParseException("Wrong key passphrase", e)
            msg.contains("corrupt") || msg.contains("invalid") ->
                ParseException("Invalid or corrupt private key", e)
            else ->
                ParseException(
                    e.message?.takeIf { it.isNotBlank() } ?: "Could not load private key",
                    e
                )
        }
    }
}
