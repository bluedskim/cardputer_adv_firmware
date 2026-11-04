package com.cardputer.companion

/**
 * Pure TOFU decision logic (unit-tested without Android / sshj).
 */
object SshTofuHostKeyLogic {

    enum class Outcome {
        /** Fingerprint matches an existing pin — proceed without prompting. */
        ALLOW_MATCH,

        /** No pin yet; caller must prompt, then pin only if user accepts. */
        NEED_TRUST_PROMPT,

        /** Pin differs — always reject; user must forget pin deliberately to re-trust. */
        REJECT_MISMATCH
    }

    fun evaluate(stored: SshHostKeyPin?, offered: SshHostKeyPin): Outcome {
        if (stored == null) return Outcome.NEED_TRUST_PROMPT
        return if (stored.sha256Hex.equals(offered.sha256Hex, ignoreCase = true)) {
            Outcome.ALLOW_MATCH
        } else {
            Outcome.REJECT_MISMATCH
        }
    }
}
