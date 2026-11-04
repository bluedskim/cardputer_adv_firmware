package com.cardputer.companion

/**
 * Blocks the SSH worker thread until the user accepts or rejects a new host key,
 * or shows mismatch UI (connect always aborted for mismatch).
 */
interface SshHostKeyTrustPrompter {
    /** @return true if the user chose to trust and pin this key. */
    fun confirmNewHost(host: String, port: Int, offered: SshHostKeyPin): Boolean

    /** Called when the server key changed; connection is already aborted. */
    fun onHostKeyMismatch(host: String, port: Int, stored: SshHostKeyPin, offered: SshHostKeyPin)
}
