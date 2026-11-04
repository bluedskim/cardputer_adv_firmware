package com.cardputer.companion

import net.schmizz.sshj.transport.verification.HostKeyVerifier
import java.security.PublicKey

class InteractiveTofuHostKeyVerifier(
    private val presetHost: String,
    private val presetPort: Int,
    private val store: SshHostKeyRepository,
    private val prompter: SshHostKeyTrustPrompter,
    private val logLine: (String) -> Unit
) : HostKeyVerifier {

    override fun verify(hostname: String?, port: Int, key: PublicKey?): Boolean {
        if (key == null) {
            logLine("host key missing")
            return false
        }
        val offered = SshHostKeyFingerprint.fromPublicKey(key)
        val shortFp = offered.sha256Hex.take(16) + "…"
        val stored = store.getPin(presetHost, presetPort)
            ?: hostname?.let { store.getPin(it, port) }

        return when (SshTofuHostKeyLogic.evaluate(stored, offered)) {
            SshTofuHostKeyLogic.Outcome.ALLOW_MATCH -> {
                logLine("host key ok ${offered.keyType} sha256=$shortFp")
                true
            }
            SshTofuHostKeyLogic.Outcome.NEED_TRUST_PROMPT -> {
                logLine("host key unknown ${offered.keyType} sha256=$shortFp — prompting")
                val accepted = prompter.confirmNewHost(presetHost, presetPort, offered)
                if (accepted) {
                    store.putPin(presetHost, presetPort, offered)
                    logLine("host key trusted and pinned")
                    true
                } else {
                    logLine("host key trust declined")
                    false
                }
            }
            SshTofuHostKeyLogic.Outcome.REJECT_MISMATCH -> {
                logLine("host key MISMATCH (pinned vs offered)")
                val pinned = stored
                if (pinned != null) {
                    prompter.onHostKeyMismatch(
                        presetHost,
                        presetPort,
                        pinned,
                        offered
                    )
                }
                false
            }
        }
    }

    override fun findExistingAlgorithms(hostname: String?, port: Int): MutableList<String> =
        mutableListOf()
}
