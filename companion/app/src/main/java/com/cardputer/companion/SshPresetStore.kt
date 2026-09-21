package com.cardputer.companion

import android.content.Context
import android.content.SharedPreferences
import androidx.security.crypto.EncryptedSharedPreferences
import androidx.security.crypto.MasterKey
import org.json.JSONArray
import org.json.JSONObject
import java.util.UUID

data class SshPreset(
    val id: String,
    val name: String,
    val host: String,
    val port: Int = 22,
    val user: String,
    val command: String,
    val password: String = ""
)

/**
 * Non-secret fields in normal prefs JSON; passwords in EncryptedSharedPreferences.
 */
class SshPresetStore(context: Context) {
    private val metaPrefs = context.getSharedPreferences(META_PREFS, Context.MODE_PRIVATE)
    private val secretPrefs: SharedPreferences = createSecretPrefs(context)

    fun load(): MutableList<SshPreset> {
        val raw = metaPrefs.getString(KEY_META, null) ?: return mutableListOf()
        return try {
            val arr = JSONArray(raw)
            MutableList(arr.length()) { i ->
                val o = arr.getJSONObject(i)
                val id = o.optString("id").ifBlank { UUID.randomUUID().toString() }
                SshPreset(
                    id = id,
                    name = o.optString("name"),
                    host = o.optString("host"),
                    port = o.optInt("port", 22).coerceIn(1, 65535),
                    user = o.optString("user"),
                    command = o.optString("command"),
                    password = secretPrefs.getString(secretKey(id), "") ?: ""
                )
            }.filter {
                it.name.isNotBlank() && it.host.isNotBlank() &&
                    it.user.isNotBlank() && it.command.isNotBlank()
            }.take(BleProtocol.SSH_MAX_PRESETS)
                .toMutableList()
        } catch (_: Exception) {
            mutableListOf()
        }
    }

    fun save(presets: List<SshPreset>) {
        val limited = presets.take(BleProtocol.SSH_MAX_PRESETS)
        val keepIds = limited.map { it.id }.toSet()
        val arr = JSONArray()
        val secretEditor = secretPrefs.edit()
        for (p in limited) {
            arr.put(
                JSONObject()
                    .put("id", p.id)
                    .put("name", p.name.take(BleProtocol.SSH_NAME_MAX))
                    .put("host", p.host)
                    .put("port", p.port.coerceIn(1, 65535))
                    .put("user", p.user)
                    .put("command", p.command)
            )
            secretEditor.putString(secretKey(p.id), p.password)
        }
        // Drop orphaned password entries.
        for (key in secretPrefs.all.keys) {
            if (key.startsWith(SECRET_PREFIX)) {
                val id = key.removePrefix(SECRET_PREFIX)
                if (id !in keepIds) secretEditor.remove(key)
            }
        }
        secretEditor.apply()
        metaPrefs.edit().putString(KEY_META, arr.toString()).apply()
    }

    companion object {
        private const val META_PREFS = "ssh_presets"
        private const val KEY_META = "presets_json"
        private const val SECRET_PREFS = "ssh_presets_secret"
        private const val SECRET_PREFIX = "pw_"

        private fun secretKey(id: String) = SECRET_PREFIX + id

        private fun createSecretPrefs(context: Context): SharedPreferences {
            val masterKey = MasterKey.Builder(context)
                .setKeyScheme(MasterKey.KeyScheme.AES256_GCM)
                .build()
            return EncryptedSharedPreferences.create(
                context,
                SECRET_PREFS,
                masterKey,
                EncryptedSharedPreferences.PrefKeyEncryptionScheme.AES256_SIV,
                EncryptedSharedPreferences.PrefValueEncryptionScheme.AES256_GCM
            )
        }
    }
}

/** TOFU host key fingerprints: "host:port" → fingerprint hex. */
class SshHostKeyStore(context: Context) {
    private val prefs = context.getSharedPreferences(PREFS, Context.MODE_PRIVATE)

    fun getFingerprint(host: String, port: Int): String? =
        prefs.getString(key(host, port), null)

    fun putFingerprint(host: String, port: Int, fingerprint: String) {
        prefs.edit().putString(key(host, port), fingerprint).apply()
    }

    companion object {
        private const val PREFS = "ssh_known_hosts"
        private fun key(host: String, port: Int) = "${host.lowercase()}:$port"
    }
}
