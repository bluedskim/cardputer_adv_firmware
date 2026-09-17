package com.cardputer.companion

import android.content.Context

/** User-facing Companion options (persisted). */
object CompanionPrefs {
    private const val PREFS = "companion_prefs"
    private const val KEY_BATTERY_SAVER = "battery_saver"

    /** Default off: prioritize reliable lock-screen Web connect. */
    fun isBatterySaverEnabled(context: Context): Boolean =
        context.getSharedPreferences(PREFS, Context.MODE_PRIVATE)
            .getBoolean(KEY_BATTERY_SAVER, false)

    fun setBatterySaverEnabled(context: Context, enabled: Boolean) {
        context.getSharedPreferences(PREFS, Context.MODE_PRIVATE)
            .edit()
            .putBoolean(KEY_BATTERY_SAVER, enabled)
            .apply()
    }
}
