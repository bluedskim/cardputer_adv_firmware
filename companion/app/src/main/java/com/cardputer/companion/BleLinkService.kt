package com.cardputer.companion

import android.app.Notification
import android.app.NotificationChannel
import android.app.NotificationManager
import android.app.PendingIntent
import android.app.Service
import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import android.content.pm.ServiceInfo
import android.os.Binder
import android.os.Build
import android.os.IBinder
import androidx.core.app.NotificationCompat
import androidx.core.content.ContextCompat
import java.util.concurrent.CopyOnWriteArrayList

/**
 * Keeps BLE listening/GATT alive while the screen is locked or the UI is backgrounded.
 * Shows an ongoing notification (required for foreground service).
 */
class BleLinkService : Service(), BleLinkClient.Listener {
    inner class LocalBinder : Binder() {
        fun getService(): BleLinkService = this@BleLinkService
    }

    private val binder = LocalBinder()
    private val uiListeners = CopyOnWriteArrayList<BleLinkClient.Listener>()
    private lateinit var store: PresetStore

    lateinit var client: BleLinkClient
        private set

    var lastStatus: String = ""
        private set
    var lastDetail: String = ""
        private set

    private val screenReceiver = object : BroadcastReceiver() {
        override fun onReceive(context: Context?, intent: Intent?) {
            when (intent?.action) {
                Intent.ACTION_SCREEN_ON,
                Intent.ACTION_SCREEN_OFF,
                Intent.ACTION_USER_PRESENT -> client.refreshScanIfListening()
            }
        }
    }

    override fun onCreate() {
        super.onCreate()
        instance = this
        store = PresetStore(this)
        client = BleLinkClient(applicationContext, this)
        startInForeground(getString(R.string.listening))
        val filter = IntentFilter().apply {
            addAction(Intent.ACTION_SCREEN_ON)
            addAction(Intent.ACTION_SCREEN_OFF)
            addAction(Intent.ACTION_USER_PRESENT)
        }
        ContextCompat.registerReceiver(
            this,
            screenReceiver,
            filter,
            ContextCompat.RECEIVER_NOT_EXPORTED
        )
    }

    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int {
        when (intent?.action) {
            ACTION_STOP -> {
                // Linked (watch-style): drop the session but keep waiting so Web → site
                // works without reopening the phone app. Presence alone is too slow/flaky.
                if (CompanionAssociation.isAssociated(this)) {
                    CompanionAssociation.startObserving(this)
                    client.disconnect(resumeListen = true)
                    return START_STICKY
                }
                client.disconnect(resumeListen = false)
                stopSelf()
                return START_NOT_STICKY
            }
            ACTION_SCAN -> client.startScan()
            ACTION_LISTEN -> client.startListening(forceRescan = false)
            ACTION_APPEAR -> {
                val addr = intent.getStringExtra(EXTRA_ADDRESS)
                if (!addr.isNullOrBlank()) {
                    client.onAssociatedDeviceAppeared(addr)
                } else {
                    client.startListening(forceRescan = false)
                }
            }
            else -> {
                if (!client.isConnected()) {
                    client.startListening(forceRescan = false)
                }
            }
        }
        return START_STICKY
    }

    override fun onBind(intent: Intent?): IBinder = binder

    override fun onDestroy() {
        try {
            unregisterReceiver(screenReceiver)
        } catch (_: IllegalArgumentException) {
        }
        client.disconnect(resumeListen = false)
        if (instance === this) {
            instance = null
        }
        super.onDestroy()
    }

    fun addUiListener(listener: BleLinkClient.Listener) {
        uiListeners.addIfAbsent(listener)
        if (lastStatus.isNotEmpty()) {
            listener.onStatus(lastStatus)
        }
        if (lastDetail.isNotEmpty()) {
            listener.onDetail(lastDetail)
        }
    }

    fun removeUiListener(listener: BleLinkClient.Listener) {
        uiListeners.remove(listener)
    }

    private fun startInForeground(statusText: String) {
        ensureChannel()
        val notification = buildNotification(statusText)
        if (Build.VERSION.SDK_INT >= 34) {
            startForeground(
                NOTIFICATION_ID,
                notification,
                ServiceInfo.FOREGROUND_SERVICE_TYPE_CONNECTED_DEVICE
            )
        } else {
            startForeground(NOTIFICATION_ID, notification)
        }
    }

    private fun ensureChannel() {
        val mgr = getSystemService(NotificationManager::class.java) ?: return
        val channel = NotificationChannel(
            CHANNEL_ID,
            getString(R.string.notif_channel_name),
            NotificationManager.IMPORTANCE_LOW
        ).apply {
            description = getString(R.string.notif_channel_desc)
            setShowBadge(false)
        }
        mgr.createNotificationChannel(channel)
    }

    private fun buildNotification(statusText: String): Notification {
        val open = PendingIntent.getActivity(
            this,
            0,
            Intent(this, MainActivity::class.java),
            PendingIntent.FLAG_UPDATE_CURRENT or PendingIntent.FLAG_IMMUTABLE
        )
        val stop = PendingIntent.getService(
            this,
            1,
            Intent(this, BleLinkService::class.java).setAction(ACTION_STOP),
            PendingIntent.FLAG_UPDATE_CURRENT or PendingIntent.FLAG_IMMUTABLE
        )
        return NotificationCompat.Builder(this, CHANNEL_ID)
            .setSmallIcon(android.R.drawable.stat_sys_data_bluetooth)
            .setContentTitle(getString(R.string.notif_title))
            .setContentText(statusText)
            .setContentIntent(open)
            .setOngoing(true)
            .setOnlyAlertOnce(true)
            .setCategory(NotificationCompat.CATEGORY_SERVICE)
            .setForegroundServiceBehavior(NotificationCompat.FOREGROUND_SERVICE_IMMEDIATE)
            .addAction(0, getString(R.string.disconnect), stop)
            .build()
    }

    private fun updateNotification(statusText: String) {
        val mgr = getSystemService(NotificationManager::class.java) ?: return
        mgr.notify(NOTIFICATION_ID, buildNotification(statusText))
    }

    override fun presets(): List<HttpPreset> = store.load()

    override fun onStatus(message: String) {
        lastStatus = message
        updateNotification(message)
        uiListeners.forEach { it.onStatus(message) }
    }

    override fun onDetail(message: String) {
        lastDetail = message
        uiListeners.forEach { it.onDetail(message) }
    }

    override fun onHello(firmwareVersion: String) {
        uiListeners.forEach { it.onHello(firmwareVersion) }
    }

    override fun onDisconnected() {
        uiListeners.forEach { it.onDisconnected() }
    }

    companion object {
        const val CHANNEL_ID = "ble_link"
        const val NOTIFICATION_ID = 42
        const val ACTION_STOP = "com.cardputer.companion.action.STOP"
        const val ACTION_LISTEN = "com.cardputer.companion.action.LISTEN"
        const val ACTION_SCAN = "com.cardputer.companion.action.SCAN"
        const val ACTION_APPEAR = "com.cardputer.companion.action.APPEAR"
        const val EXTRA_ADDRESS = "address"

        @Volatile
        var instance: BleLinkService? = null
            private set

        fun startListening(context: Context) {
            CompanionAssociation.startObserving(context)
            val intent = Intent(context, BleLinkService::class.java).setAction(ACTION_LISTEN)
            ContextCompat.startForegroundService(context, intent)
        }

        fun startScan(context: Context) {
            val intent = Intent(context, BleLinkService::class.java).setAction(ACTION_SCAN)
            ContextCompat.startForegroundService(context, intent)
        }

        /** Called from CompanionDeviceService when linked CardputerAdv is nearby. */
        fun onAssociatedDeviceAppeared(context: Context, address: String) {
            CompanionAssociation.saveAddress(context, address)
            CompanionAssociation.startObserving(context)
            val intent = Intent(context, BleLinkService::class.java)
                .setAction(ACTION_APPEAR)
                .putExtra(EXTRA_ADDRESS, address)
            ContextCompat.startForegroundService(context, intent)
        }

        fun stop(context: Context) {
            if (CompanionAssociation.isAssociated(context)) {
                CompanionAssociation.startObserving(context)
                val running = instance
                if (running != null) {
                    running.client.disconnect(resumeListen = true)
                } else {
                    startListening(context)
                }
                return
            }
            val running = instance
            if (running != null) {
                running.client.disconnect(resumeListen = false)
                running.stopSelf()
            } else {
                context.stopService(Intent(context, BleLinkService::class.java))
            }
        }

        fun unlink(context: Context) {
            stop(context)
            CompanionAssociation.disassociateAll(context)
        }
    }
}
