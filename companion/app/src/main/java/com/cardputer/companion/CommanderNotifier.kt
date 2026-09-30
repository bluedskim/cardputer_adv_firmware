package com.cardputer.companion

import android.app.NotificationChannel
import android.app.NotificationManager
import android.app.PendingIntent
import android.content.Context
import android.content.Intent
import android.os.Build
import androidx.core.app.NotificationCompat
import androidx.core.app.NotificationManagerCompat

/**
 * User-visible alert when a Cardputer Web/SSH run finishes on the phone.
 * Success notifications include the truncated result body (≤512 bytes).
 */
object CommanderNotifier {
    private const val CHANNEL_ID = "commander_result"
    private const val NOTIFICATION_ID = 43

    fun notifyHttp(context: Context, presetName: String, result: HttpExecResult) {
        val app = context.applicationContext
        ensureChannel(app)
        val ok = result.errCode == 0
        val title = if (ok) {
            app.getString(R.string.commander_http_ok_title, presetName, result.httpStatus)
        } else {
            app.getString(
                R.string.commander_http_err_title,
                presetName,
                httpErrLabel(app, result.errCode)
            )
        }
        val body = if (ok) bodyText(app, result.body) else null
        post(app, title, body, ok)
    }

    fun notifySsh(context: Context, presetName: String, result: SshExecResult) {
        val app = context.applicationContext
        ensureChannel(app)
        val ok = result.errCode == 0
        val title = if (ok) {
            app.getString(R.string.commander_ssh_ok_title, presetName, result.exitStatus)
        } else {
            app.getString(
                R.string.commander_ssh_err_title,
                presetName,
                sshErrLabel(app, result.errCode)
            )
        }
        val body = if (ok) bodyText(app, result.body) else null
        post(app, title, body, ok)
    }

    private fun bodyText(context: Context, bytes: ByteArray): String {
        if (bytes.isEmpty()) {
            return context.getString(R.string.test_run_empty_body)
        }
        // Replace unpaired surrogates / control junk lightly for notification text.
        return String(bytes, Charsets.UTF_8)
            .replace('\u0000', ' ')
            .trim()
            .ifEmpty { context.getString(R.string.test_run_empty_body) }
    }

    private fun httpErrLabel(context: Context, err: Int): String = when (err) {
        BleProtocol.ERR_HTTP_TIMEOUT -> context.getString(R.string.http_err_timeout)
        BleProtocol.ERR_HTTP_FAILED -> context.getString(R.string.http_err_failed)
        else -> context.getString(R.string.http_err_failed)
    }

    private fun sshErrLabel(context: Context, err: Int): String = when (err) {
        BleProtocol.ERR_SSH_TIMEOUT -> context.getString(R.string.ssh_err_timeout)
        BleProtocol.ERR_SSH_AUTH_FAILED -> context.getString(R.string.ssh_err_auth)
        BleProtocol.ERR_SSH_HOST_KEY -> context.getString(R.string.ssh_err_host_key)
        BleProtocol.ERR_SSH_FAILED -> context.getString(R.string.ssh_err_failed)
        else -> context.getString(R.string.ssh_err_failed)
    }

    private fun ensureChannel(context: Context) {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.O) {
            return
        }
        val nm = context.getSystemService(NotificationManager::class.java) ?: return
        val channel = NotificationChannel(
            CHANNEL_ID,
            context.getString(R.string.commander_notif_channel_name),
            NotificationManager.IMPORTANCE_DEFAULT
        ).apply {
            description = context.getString(R.string.commander_notif_channel_desc)
        }
        nm.createNotificationChannel(channel)
    }

    private fun post(context: Context, title: String, bigText: String?, success: Boolean) {
        val open = PendingIntent.getActivity(
            context,
            0,
            Intent(context, MainActivity::class.java),
            PendingIntent.FLAG_UPDATE_CURRENT or PendingIntent.FLAG_IMMUTABLE
        )
        val builder = NotificationCompat.Builder(context, CHANNEL_ID)
            .setSmallIcon(android.R.drawable.stat_sys_data_bluetooth)
            .setContentTitle(title)
            .setContentText(
                bigText?.lineSequence()?.firstOrNull()?.take(80)
                    ?: context.getString(
                        if (success) R.string.commander_notif_tap else R.string.commander_notif_failed
                    )
            )
            .setContentIntent(open)
            .setAutoCancel(true)
            .setCategory(NotificationCompat.CATEGORY_STATUS)
            .setPriority(
                if (success) NotificationCompat.PRIORITY_DEFAULT
                else NotificationCompat.PRIORITY_HIGH
            )
        if (!bigText.isNullOrEmpty()) {
            builder.setStyle(NotificationCompat.BigTextStyle().bigText(bigText))
        }
        try {
            NotificationManagerCompat.from(context).notify(NOTIFICATION_ID, builder.build())
        } catch (_: SecurityException) {
            // POST_NOTIFICATIONS denied — link FGS still works; skip the alert.
        }
    }
}
