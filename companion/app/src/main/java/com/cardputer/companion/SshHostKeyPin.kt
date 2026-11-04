package com.cardputer.companion

import java.security.MessageDigest
import java.security.PublicKey
import java.util.Base64

/** Pinned server host key for a host:port (fingerprints are not secret). */
data class SshHostKeyPin(
    val keyType: String,
    val sha256Hex: String
) {
    fun displayFingerprint(): String = SshHostKeyFingerprint.formatSha256Base64(sha256Hex)

    /** Stored form: `keyType|sha256hex` (no secrets). */
    fun toStorageString(): String = "${keyType.replace('|', '_')}|${sha256Hex.lowercase()}"

    companion object {
        fun fromStorageString(raw: String): SshHostKeyPin? {
            val trimmed = raw.trim()
            if (trimmed.isEmpty()) return null
            if (trimmed.startsWith("{")) {
                return parseJsonLegacy(trimmed)
            }
            if (trimmed.contains("|")) {
                val parts = trimmed.split("|", limit = 2)
                if (parts.size == 2 && parts[1].matches(Regex("(?i)[0-9a-f]{64}"))) {
                    return SshHostKeyPin(parts[0], parts[1].lowercase())
                }
                return null
            }
            if (trimmed.matches(Regex("(?i)[0-9a-f]{64}"))) {
                return SshHostKeyPin("ssh-unknown", trimmed.lowercase())
            }
            return null
        }

        private fun parseJsonLegacy(raw: String): SshHostKeyPin? {
            val typeMatch = Regex(""""type"\s*:\s*"([^"]+)"""")
                .find(raw)?.groupValues?.getOrNull(1)
                ?.ifBlank { "ssh-unknown" } ?: "ssh-unknown"
            val sha = Regex(""""sha256"\s*:\s*"([^"]+)"""")
                .find(raw)?.groupValues?.getOrNull(1)
                ?: return null
            if (!sha.matches(Regex("(?i)[0-9a-f]{64}"))) return null
            return SshHostKeyPin(typeMatch, sha.lowercase())
        }
    }
}

object SshHostKeyFingerprint {
    fun fromPublicKey(key: PublicKey): SshHostKeyPin {
        val digest = MessageDigest.getInstance("SHA-256").digest(key.encoded)
        val hex = digest.joinToString("") { b -> "%02x".format(b) }
        return SshHostKeyPin(keyTypeLabel(key), hex)
    }

    fun formatSha256Base64(sha256Hex: String): String {
        val bytes = sha256Hex.chunked(2).map { it.toInt(16).toByte() }.toByteArray()
        val b64 = Base64.getEncoder().withoutPadding().encodeToString(bytes)
        return "SHA256:$b64"
    }

    fun keyTypeLabel(key: PublicKey): String = when (key.algorithm.uppercase()) {
        "ED25519", "EDDSA" -> "ssh-ed25519"
        "RSA" -> "ssh-rsa"
        "EC", "ECDSA" -> "ssh-ecdsa"
        "DSA" -> "ssh-dss"
        else -> key.algorithm.lowercase()
    }
}
