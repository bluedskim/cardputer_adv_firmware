package com.cardputer.companion

/** Read/write pinned SSH host keys (fingerprints are not secret). */
interface SshHostKeyRepository {
    fun getPin(host: String, port: Int): SshHostKeyPin?
    fun putPin(host: String, port: Int, pin: SshHostKeyPin)
    fun remove(host: String, port: Int)
}
