package com.cardputer.companion

import android.app.Notification
import android.app.NotificationChannel
import android.app.NotificationManager
import android.app.PendingIntent
import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import android.media.Ringtone
import android.media.RingtoneManager
import android.os.Build
import android.os.Handler
import android.os.Looper
import android.os.VibrationEffect
import android.os.Vibrator
import android.os.VibratorManager
import androidx.core.app.NotificationCompat
import androidx.core.content.ContextCompat

/**
 * Vibrate-first Find My Phone session. Ringtone only when [sound] is true.
 */
object FindAlerter {
    const val ACTION_STOP = "com.cardputer.companion.FIND_STOP"
    private const val CHANNEL_ID = "find_phone"
    private const val NOTIFICATION_ID = 42
    private const val TIMEOUT_MS = 60_000L

    private val handler = Handler(Looper.getMainLooper())
    private var active = false
    private var ringtone: Ringtone? = null
    private var onStopped: ((Int) -> Unit)? = null
    private var registered = false

    private val stopReceiver = object : BroadcastReceiver() {
        override fun onReceive(context: Context?, intent: Intent?) {
            if (intent?.action == ACTION_STOP) {
                finish(context ?: return, BleProtocol.FIND_REASON_USER)
            }
        }
    }

    private val timeoutRunnable = Runnable {
        // Context is captured via last start; timeout uses stored callback only.
        finishTimeout()
    }

    private var appContext: Context? = null

    fun isActive(): Boolean = active

    fun start(context: Context, sound: Boolean, onStopped: (reason: Int) -> Unit) {
        val app = context.applicationContext
        stop(app, notify = false)
        appContext = app
        this.onStopped = onStopped
        ensureChannel(app)
        register(app)
        vibrate(app)
        if (sound) {
            playSound(app)
        }
        postNotification(app)
        active = true
        handler.postDelayed(timeoutRunnable, TIMEOUT_MS)
    }

    fun stop(context: Context, notify: Boolean = true) {
        if (!active && ringtone == null) {
            return
        }
        finish(context.applicationContext, BleProtocol.FIND_REASON_USER, notify)
    }

    private fun finishTimeout() {
        val ctx = appContext ?: return
        finish(ctx, BleProtocol.FIND_REASON_TIMEOUT)
    }

    private fun finish(context: Context, reason: Int, notify: Boolean = true) {
        if (!active && ringtone == null) {
            return
        }
        handler.removeCallbacks(timeoutRunnable)
        active = false
        ringtone?.stop()
        ringtone = null
        cancelVibrate(context)
        val nm = context.getSystemService(NotificationManager::class.java)
        nm?.cancel(NOTIFICATION_ID)
        unregister(context)
        if (notify) {
            val cb = onStopped
            onStopped = null
            cb?.invoke(reason)
        } else {
            onStopped = null
        }
    }

    private fun register(context: Context) {
        if (registered) return
        val filter = IntentFilter(ACTION_STOP)
        ContextCompat.registerReceiver(
            context,
            stopReceiver,
            filter,
            ContextCompat.RECEIVER_NOT_EXPORTED
        )
        registered = true
    }

    private fun unregister(context: Context) {
        if (!registered) return
        try {
            context.unregisterReceiver(stopReceiver)
        } catch (_: IllegalArgumentException) {
        }
        registered = false
    }

    private fun ensureChannel(context: Context) {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.O) return
        val nm = context.getSystemService(NotificationManager::class.java) ?: return
        val channel = NotificationChannel(
            CHANNEL_ID,
            "Find phone",
            NotificationManager.IMPORTANCE_HIGH
        ).apply {
            description = "Cardputer Find My Phone"
            enableVibration(true)
            setSound(null, null)
        }
        nm.createNotificationChannel(channel)
    }

    private fun vibrate(context: Context) {
        val pattern = longArrayOf(0, 600, 400)
        val vibrator = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            val vm = context.getSystemService(VibratorManager::class.java)
            vm?.defaultVibrator
        } else {
            @Suppress("DEPRECATION")
            context.getSystemService(Context.VIBRATOR_SERVICE) as? Vibrator
        } ?: return
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            vibrator.vibrate(VibrationEffect.createWaveform(pattern, 0))
        } else {
            @Suppress("DEPRECATION")
            vibrator.vibrate(pattern, 0)
        }
    }

    private fun cancelVibrate(context: Context) {
        val vibrator = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            context.getSystemService(VibratorManager::class.java)?.defaultVibrator
        } else {
            @Suppress("DEPRECATION")
            context.getSystemService(Context.VIBRATOR_SERVICE) as? Vibrator
        }
        vibrator?.cancel()
    }

    private fun playSound(context: Context) {
        val uri = RingtoneManager.getDefaultUri(RingtoneManager.TYPE_ALARM)
            ?: RingtoneManager.getDefaultUri(RingtoneManager.TYPE_RINGTONE)
            ?: return
        ringtone = RingtoneManager.getRingtone(context, uri)?.apply {
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
                isLooping = true
            }
            play()
        }
    }

    private fun postNotification(context: Context) {
        val stop = Intent(ACTION_STOP).setPackage(context.packageName)
        val pi = PendingIntent.getBroadcast(
            context,
            0,
            stop,
            PendingIntent.FLAG_UPDATE_CURRENT or PendingIntent.FLAG_IMMUTABLE
        )
        val notification: Notification = NotificationCompat.Builder(context, CHANNEL_ID)
            .setSmallIcon(android.R.drawable.stat_sys_warning)
            .setContentTitle("Find phone")
            .setContentText("Cardputer is looking for this phone")
            .setOngoing(true)
            .setPriority(NotificationCompat.PRIORITY_HIGH)
            .addAction(0, "Stop", pi)
            .build()
        val nm = context.getSystemService(NotificationManager::class.java)
        nm?.notify(NOTIFICATION_ID, notification)
    }
}
