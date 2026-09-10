package com.cardputer.companion

import android.content.Context
import org.json.JSONArray
import org.json.JSONObject

data class HttpPreset(
    val name: String,
    val url: String,
    val method: String = "GET",
    val headers: String = "", // one "Key: Value" per line
    val body: String = ""
)

class PresetStore(context: Context) {
    private val prefs = context.getSharedPreferences(PREFS, Context.MODE_PRIVATE)

    fun load(): MutableList<HttpPreset> {
        val raw = prefs.getString(KEY, null) ?: return defaultPresets().toMutableList()
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
        prefs.edit().putString(KEY, arr.toString()).apply()
    }

    companion object {
        private const val PREFS = "http_presets"
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
    }
}
