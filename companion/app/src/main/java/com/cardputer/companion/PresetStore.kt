package com.cardputer.companion

import android.content.Context
import android.content.SharedPreferences
import androidx.security.crypto.EncryptedSharedPreferences
import androidx.security.crypto.MasterKey
import org.json.JSONArray
import org.json.JSONObject

data class HttpPreset(
    val name: String,
    val url: String,
    val method: String = "GET",
    val headers: String = "", // one "Key: Value" per line
    val body: String = ""
)

/**
 * Preset JSON (URL, headers, body — may hold tokens) lives in encrypted prefs.
 * A previous install stored the same JSON in plain SharedPreferences; the first
 * load copies it across and deletes the plain key.
 */
class PresetStore(context: Context) {
    private val plainPrefs = context.getSharedPreferences(PREFS, Context.MODE_PRIVATE)
    private val secretPrefs: SharedPreferences = createSecretPrefs(context)

    fun load(): MutableList<HttpPreset> {
        val encrypted = secretPrefs.getString(KEY, null)
        if (encrypted != null) {
            return parse(encrypted)
        }
        val legacy = plainPrefs.getString(KEY, null) ?: return defaultPresets().toMutableList()
        val loaded = parse(legacy)
        save(loaded)
        return loaded
    }

    fun save(presets: List<HttpPreset>) {
        val arr = JSONArray()
        for (p in presets.take(BleProtocol.HTTP_MAX_PRESETS)) {
            arr.put(
                JSONObject()
                    .put("name", p.name.take(BleProtocol.HTTP_NAME_MAX))
                    .put("url", p.url)
                    .put("method", p.method.uppercase())
                    .put("headers", p.headers)
                    .put("body", p.body)
            )
        }
        secretPrefs.edit().putString(KEY, arr.toString()).apply()
        plainPrefs.edit().remove(KEY).apply()
    }

    private fun parse(raw: String): MutableList<HttpPreset> {
        return try {
            val arr = JSONArray(raw)
            val loaded = MutableList(arr.length()) { i ->
                val o = arr.getJSONObject(i)
                HttpPreset(
                    name = o.optString("name"),
                    url = o.optString("url"),
                    method = o.optString("method", "GET"),
                    headers = o.optString("headers"),
                    body = o.optString("body")
                )
            }.filter { it.name.isNotBlank() && it.url.isNotBlank() }
                .toMutableList()
            if (loaded.isEmpty()) defaultPresets().toMutableList() else loaded
        } catch (_: Exception) {
            defaultPresets().toMutableList()
        }
    }

    companion object {
        private const val PREFS = "http_presets"
        private const val SECRET_PREFS = "http_presets_secret"
        private const val KEY = "presets_json"

        fun defaultPresets(): List<HttpPreset> = listOf(
            HttpPreset(
                name = "ipify",
                url = "https://api.ipify.org?format=json",
                method = "GET"
            ),
            HttpPreset(
                name = "httpbin get",
                url = "https://httpbin.org/get",
                method = "GET"
            )
        )

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
