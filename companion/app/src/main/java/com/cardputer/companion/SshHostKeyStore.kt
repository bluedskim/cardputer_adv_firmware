package com.cardputer.companion

import android.content.Context
import android.content.SharedPreferences

/** TOFU host key pins: host:port → key type + SHA-256 fingerprint (hex). */
class SshHostKeyStore(context: Context) : SshHostKeyRepository {
    private val prefs: SharedPreferences =
        context.getSharedPreferences(PREFS, Context.MODE_PRIVATE)

    override fun getPin(host: String, port: Int): SshHostKeyPin? {
        val raw = prefs.getString(storageKey(host, port), null) ?: return null
        return SshHostKeyPin.fromStorageString(raw)
    }

    /** @deprecated Use [getPin]; kept for migration tests. */
    fun getFingerprint(host: String, port: Int): String? = getPin(host, port)?.sha256Hex

    override fun putPin(host: String, port: Int, pin: SshHostKeyPin) {
        prefs.edit().putString(storageKey(host, port), pin.toStorageString()).apply()
    }

    fun putFingerprint(host: String, port: Int, fingerprint: String) {
        putPin(host, port, SshHostKeyPin("ssh-unknown", fingerprint.lowercase()))
    }

    override fun remove(host: String, port: Int) {
        prefs.edit().remove(storageKey(host, port)).apply()
    }

    fun hasPin(host: String, port: Int): Boolean = prefs.contains(storageKey(host, port))

    companion object {
        private const val PREFS = "ssh_known_hosts"

        internal fun storageKey(host: String, port: Int): String = "${host.lowercase()}:$port"
    }
}
