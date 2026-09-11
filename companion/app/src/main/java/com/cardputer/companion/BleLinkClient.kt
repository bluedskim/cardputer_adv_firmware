package com.cardputer.companion

import android.annotation.SuppressLint
import android.bluetooth.BluetoothAdapter
import android.bluetooth.BluetoothDevice
import android.bluetooth.BluetoothGatt
import android.bluetooth.BluetoothGattCallback
import android.bluetooth.BluetoothGattCharacteristic
import android.bluetooth.BluetoothGattDescriptor
import android.bluetooth.BluetoothManager
import android.bluetooth.BluetoothProfile
import android.bluetooth.le.ScanCallback
import android.bluetooth.le.ScanFilter
import android.bluetooth.le.ScanResult
import android.bluetooth.le.ScanSettings
import android.content.Context
import android.os.Build
import android.os.Handler
import android.os.Looper
import android.os.ParcelUuid
import android.os.PowerManager
import java.util.ArrayDeque
import java.util.UUID
import java.util.concurrent.atomic.AtomicInteger

class BleLinkClient(
    private val context: Context,
    private val listener: Listener
) {
    interface Listener {
        fun onStatus(message: String)
        fun onDetail(message: String)
        fun onHello(firmwareVersion: String)
        fun onDisconnected()
        fun presets(): List<HttpPreset>
    }

    private val handler = Handler(Looper.getMainLooper())
    private val bluetoothManager =
        context.getSystemService(Context.BLUETOOTH_SERVICE) as BluetoothManager
    private val adapter: BluetoothAdapter? = bluetoothManager.adapter
    private val prefs = context.getSharedPreferences(PREFS, Context.MODE_PRIVATE)
    private val wakeLock: PowerManager.WakeLock =
        (context.getSystemService(Context.POWER_SERVICE) as PowerManager)
            .newWakeLock(PowerManager.PARTIAL_WAKE_LOCK, "CardputerCompanion:BleLink")
            .apply { setReferenceCounted(false) }
    private val nextReqId = AtomicInteger(1)

    private var gatt: BluetoothGatt? = null
    private var txCharacteristic: BluetoothGattCharacteristic? = null
    private var rxCharacteristic: BluetoothGattCharacteristic? = null
    private var scanning = false
    private var listenWanted = false
    private var connecting = false
    /** When linked via Companion Device Manager, prefer OS autoConnect + presence. */
    private var preferAutoConnect = false
    private var writeBusy = false
    private val writeQueue: ArrayDeque<ByteArray> = ArrayDeque()
    private val waitHintRunnable = Runnable {
        if ((scanning || preferAutoConnect) && gatt == null) {
            listener.onStatus(context.getString(R.string.waiting_device_hint))
        }
    }
    private val resumeListenRunnable = Runnable {
        if (listenWanted && gatt == null && !connecting) {
            startListening(forceRescan = false)
        }
    }
    /** Screen-off / OEM stacks often silently stop callback scans; restart while listening. */
    private val scanWatchdogRunnable = object : Runnable {
        override fun run() {
            if (!listenWanted || gatt != null) return
            if (preferAutoConnect) {
                val addr = peerAddress()
                if (addr != null) {
                    connecting = false
                    tearDownGatt()
                    connectToAddress(addr, autoConnect = true)
                } else {
                    beginScan(continuous = true)
                }
            } else if (!connecting) {
                beginScan(continuous = true)
            }
            handler.postDelayed(this, SCAN_WATCHDOG_MS)
        }
    }
    /** Firmware idle-disconnects after ~60s without traffic; keep the GATT warm. */
    private val keepAliveRunnable = object : Runnable {
        override fun run() {
            if (gatt == null) return
            if (rxCharacteristic == null) {
                handler.postDelayed(this, KEEP_ALIVE_MS)
                return
            }
            enqueueWrite(
                BleProtocol.encode(BleProtocol.TYPE_PING, nextReqId.getAndIncrement() and 0xFFFF)
            )
            handler.postDelayed(this, KEEP_ALIVE_MS)
        }
    }

    private val serviceUuid = UUID.fromString(BleProtocol.SERVICE_UUID)
    private val rxUuid = UUID.fromString(BleProtocol.RX_UUID)
    private val txUuid = UUID.fromString(BleProtocol.TX_UUID)
    private val cccdUuid = UUID.fromString("00002902-0000-1000-8000-00805f9b34fb")

    fun isBluetoothEnabled(): Boolean = adapter?.isEnabled == true

    fun isConnected(): Boolean = gatt != null

    /** Last known Cardputer MAC (uppercase), if any. */
    fun lastPeerAddress(): String? = peerAddress()

    /** Foreground auto-reconnect: presence/autoConnect when linked, else continuous scan. */
    @SuppressLint("MissingPermission")
    fun startListening(forceRescan: Boolean = false) {
        val bt = adapter
        if (bt == null || !bt.isEnabled) {
            listener.onStatus(context.getString(R.string.bt_off))
            return
        }
        listenWanted = true
        acquireWakeLock()
        CompanionAssociation.startObserving(context)
        if (!forceRescan && gatt != null) {
            scheduleBackgroundJobs()
            return
        }
        if (forceRescan) {
            tearDownGatt()
        }
        val linked = CompanionAssociation.primaryAddress(context) ?: rememberedAddress()
        preferAutoConnect = linked != null && CompanionAssociation.isAssociated(context)
        if (preferAutoConnect && linked != null && !forceRescan) {
            listener.onStatus(context.getString(R.string.listening_linked))
            listener.onDetail("linked=$linked")
            connectToAddress(linked, autoConnect = true)
            scheduleBackgroundJobs()
            handler.removeCallbacks(waitHintRunnable)
            handler.postDelayed(waitHintRunnable, 12_000)
        } else {
            preferAutoConnect = false
            beginScan(continuous = true)
            scheduleBackgroundJobs()
        }
    }

    /** Stop advertising scan only; keeps an existing GATT link. */
    @SuppressLint("MissingPermission")
    fun stopListening() {
        listenWanted = false
        preferAutoConnect = false
        clearBackgroundJobs()
        stopScan()
        if (gatt == null) {
            releaseWakeLock()
            listener.onStatus(context.getString(R.string.status_idle))
        }
    }

    /** Manual Scan button: disconnect and scan again (pairing / force). */
    @SuppressLint("MissingPermission")
    fun startScan() {
        listenWanted = true
        preferAutoConnect = false
        acquireWakeLock()
        CompanionAssociation.startObserving(context)
        handler.removeCallbacks(resumeListenRunnable)
        tearDownGatt()
        beginScan(continuous = true)
        scheduleBackgroundJobs()
    }

    /**
     * System companion presence: CardputerAdv advertised nearby (lock screen / background).
     */
    @SuppressLint("MissingPermission")
    fun onAssociatedDeviceAppeared(address: String) {
        CompanionAssociation.saveAddress(context, address)
        rememberAddress(address)
        listenWanted = true
        preferAutoConnect = true
        acquireWakeLock()
        if (gatt != null) {
            scheduleBackgroundJobs()
            return
        }
        handler.removeCallbacks(resumeListenRunnable)
        connecting = false
        tearDownGatt()
        stopScan()
        connectToAddress(address, autoConnect = false)
        scheduleBackgroundJobs()
    }

    /**
     * Drop the link. [resumeListen] false = stay idle until Scan / presence
     * (use for explicit Disconnect so Settings disconnect does not bounce back).
     */
    @SuppressLint("MissingPermission")
    fun disconnect(resumeListen: Boolean = false) {
        clearBackgroundJobs()
        stopScan()
        tearDownGatt()
        if (resumeListen && adapter?.isEnabled == true) {
            listenWanted = true
            acquireWakeLock()
            handler.postDelayed(resumeListenRunnable, RESUME_LISTEN_DELAY_MS)
            listener.onStatus(
                if (CompanionAssociation.isAssociated(context)) {
                    context.getString(R.string.listening_linked)
                } else {
                    context.getString(R.string.listening)
                }
            )
        } else {
            listenWanted = false
            preferAutoConnect = false
            releaseWakeLock()
            listener.onStatus(context.getString(R.string.disconnected))
        }
    }

    /** Call when screen turns on/off so OEM-throttled scans resume. */
    fun refreshScanIfListening() {
        if (!listenWanted || gatt != null || connecting) return
        if (preferAutoConnect) {
            val addr = peerAddress() ?: return
            connectToAddress(addr, autoConnect = true)
        } else {
            beginScan(continuous = true)
        }
        scheduleBackgroundJobs()
    }

    @SuppressLint("MissingPermission")
    fun connectToAddress(address: String, autoConnect: Boolean) {
        val bt = adapter ?: return
        if (gatt != null || connecting) return
        val normalized = CompanionAssociation.normalizeBluetoothAddress(address)
        if (normalized == null) {
            listener.onStatus("Bad address: $address")
            return
        }
        connecting = true
        stopScan()
        handler.removeCallbacks(waitHintRunnable)
        listener.onStatus(context.getString(R.string.connecting))
        try {
            val device = if (Build.VERSION.SDK_INT >= 33) {
                // CardputerAdv uses a random LE address; try random then public.
                try {
                    bt.getRemoteLeDevice(normalized, BluetoothDevice.ADDRESS_TYPE_RANDOM)
                } catch (_: IllegalArgumentException) {
                    bt.getRemoteLeDevice(normalized, BluetoothDevice.ADDRESS_TYPE_PUBLIC)
                }
            } else {
                bt.getRemoteDevice(normalized)
            }
            gatt = device.connectGatt(
                context,
                autoConnect,
                gattCallback,
                BluetoothDevice.TRANSPORT_LE
            )
        } catch (e: IllegalArgumentException) {
            connecting = false
            listener.onStatus("Bad address: $normalized")
        }
    }

    private fun peerAddress(): String? =
        CompanionAssociation.primaryAddress(context) ?: rememberedAddress()

    @SuppressLint("MissingPermission")
    private fun beginScan(continuous: Boolean) {
        val bt = adapter ?: return
        stopScan()
        scanning = true
        connecting = false
        listener.onStatus(context.getString(R.string.listening))
        listener.onDetail(rememberedAddress()?.let { "last=$it" } ?: "")

        val filters = buildScanFilters()
        // Filtered + aggressive match: required for many OEMs when the screen is off.
        // LOW_LATENCY still helps discovery; scanWatchdog restarts if the stack stalls.
        val settings = ScanSettings.Builder()
            .setScanMode(
                if (continuous) ScanSettings.SCAN_MODE_LOW_LATENCY
                else ScanSettings.SCAN_MODE_BALANCED
            )
            .setCallbackType(ScanSettings.CALLBACK_TYPE_ALL_MATCHES)
            .setMatchMode(ScanSettings.MATCH_MODE_AGGRESSIVE)
            .setNumOfMatches(ScanSettings.MATCH_NUM_MAX_ADVERTISEMENT)
            .setReportDelay(0)
            .build()

        try {
            bt.bluetoothLeScanner?.startScan(filters, settings, scanCallback)
        } catch (e: IllegalStateException) {
            scanning = false
            listener.onStatus("Scan error: ${e.message}")
            return
        }
        handler.removeCallbacks(waitHintRunnable)
        handler.postDelayed(waitHintRunnable, 12_000)
    }

    private fun scheduleBackgroundJobs() {
        acquireWakeLock()
        handler.removeCallbacks(scanWatchdogRunnable)
        handler.removeCallbacks(keepAliveRunnable)
        when {
            listenWanted && gatt == null ->
                handler.postDelayed(scanWatchdogRunnable, SCAN_WATCHDOG_MS)
            gatt != null ->
                handler.postDelayed(keepAliveRunnable, KEEP_ALIVE_MS)
        }
    }

    private fun clearBackgroundJobs() {
        handler.removeCallbacks(waitHintRunnable)
        handler.removeCallbacks(resumeListenRunnable)
        handler.removeCallbacks(scanWatchdogRunnable)
        handler.removeCallbacks(keepAliveRunnable)
    }

    private fun acquireWakeLock() {
        try {
            // Re-acquire extends timeout while we are still listening/connected.
            wakeLock.acquire(WAKE_LOCK_TIMEOUT_MS)
        } catch (_: RuntimeException) {
            // Some devices reject wake locks under policy; BLE FGS still helps.
        }
    }

    private fun releaseWakeLock() {
        try {
            if (wakeLock.isHeld) wakeLock.release()
        } catch (_: RuntimeException) {
        }
    }

    @SuppressLint("MissingPermission")
    private fun buildScanFilters(): List<ScanFilter> {
        val filters = mutableListOf<ScanFilter>()
        val addr = rememberedAddress()
        if (addr != null) {
            try {
                filters.add(ScanFilter.Builder().setDeviceAddress(addr).build())
            } catch (_: IllegalArgumentException) {
                // fall through to name filter
            }
        }
        filters.add(
            ScanFilter.Builder()
                .setDeviceName(BleProtocol.DEVICE_NAME)
                .build()
        )
        filters.add(
            ScanFilter.Builder()
                .setServiceUuid(ParcelUuid(serviceUuid))
                .build()
        )
        return filters
    }

    @SuppressLint("MissingPermission")
    private fun stopScan() {
        if (!scanning) return
        scanning = false
        adapter?.bluetoothLeScanner?.stopScan(scanCallback)
    }

    @SuppressLint("MissingPermission")
    private fun tearDownGatt() {
        writeQueue.clear()
        writeBusy = false
        connecting = false
        txCharacteristic = null
        rxCharacteristic = null
        gatt?.disconnect()
        gatt?.close()
        gatt = null
    }

    @SuppressLint("MissingPermission")
    private fun connect(device: BluetoothDevice) {
        if (connecting || gatt != null) return
        connecting = true
        stopScan()
        handler.removeCallbacks(waitHintRunnable)
        listener.onStatus(context.getString(R.string.connecting))
        gatt = device.connectGatt(context, false, gattCallback, BluetoothDevice.TRANSPORT_LE)
    }

    private fun rememberedAddress(): String? =
        CompanionAssociation.normalizeBluetoothAddress(prefs.getString(KEY_LAST_ADDR, null))

    private fun rememberAddress(address: String) {
        val normalized = CompanionAssociation.normalizeBluetoothAddress(address) ?: return
        prefs.edit().putString(KEY_LAST_ADDR, normalized).apply()
        CompanionAssociation.saveAddress(context, normalized)
    }

    @SuppressLint("MissingPermission")
    private fun isLikelyOurDevice(device: BluetoothDevice, result: ScanResult): Boolean {
        val name = device.name ?: result.scanRecord?.deviceName
        if (name == BleProtocol.DEVICE_NAME) return true
        val addr = rememberedAddress()
        if (addr != null && device.address.equals(addr, ignoreCase = true)) return true
        val uuids = result.scanRecord?.serviceUuids
        if (uuids != null && uuids.any { it.uuid == serviceUuid }) return true
        return false
    }

    private val scanCallback = object : ScanCallback() {
        @SuppressLint("MissingPermission")
        override fun onScanResult(callbackType: Int, result: ScanResult) {
            if (!isLikelyOurDevice(result.device, result)) return
            connect(result.device)
        }

        override fun onScanFailed(errorCode: Int) {
            scanning = false
            listener.onStatus("Scan failed: $errorCode")
            if (listenWanted && gatt == null) {
                handler.removeCallbacks(scanWatchdogRunnable)
                handler.postDelayed(scanWatchdogRunnable, SCAN_RETRY_AFTER_FAIL_MS)
            }
        }
    }

    private val gattCallback = object : BluetoothGattCallback() {
        @SuppressLint("MissingPermission")
        override fun onConnectionStateChange(gatt: BluetoothGatt, status: Int, newState: Int) {
            if (newState == BluetoothProfile.STATE_CONNECTED) {
                connecting = false
                rememberAddress(gatt.device.address)
                try {
                    gatt.requestConnectionPriority(BluetoothGatt.CONNECTION_PRIORITY_HIGH)
                } catch (_: SecurityException) {
                }
                handler.post {
                    listener.onStatus(context.getString(R.string.connected_waiting_hello))
                    scheduleBackgroundJobs()
                }
                gatt.discoverServices()
            } else if (newState == BluetoothProfile.STATE_DISCONNECTED) {
                connecting = false
                writeQueue.clear()
                writeBusy = false
                gatt.close()
                if (this@BleLinkClient.gatt === gatt) {
                    this@BleLinkClient.gatt = null
                    txCharacteristic = null
                    rxCharacteristic = null
                }
                handler.post {
                    listener.onStatus(context.getString(R.string.disconnected))
                    listener.onDisconnected()
                    // Delay before re-listen so we do not catch stale advertisements
                    // right after an intentional device Disconnect.
                    if (listenWanted) {
                        acquireWakeLock()
                        handler.removeCallbacks(resumeListenRunnable)
                        handler.postDelayed(resumeListenRunnable, RESUME_LISTEN_DELAY_MS)
                    } else {
                        releaseWakeLock()
                    }
                }
            }
        }

        @SuppressLint("MissingPermission")
        override fun onServicesDiscovered(gatt: BluetoothGatt, status: Int) {
            if (status != BluetoothGatt.GATT_SUCCESS) {
                handler.post { listener.onStatus("Service discovery failed: $status") }
                return
            }
            val service = gatt.getService(serviceUuid)
            if (service == null) {
                handler.post { listener.onStatus("Link service missing") }
                return
            }
            txCharacteristic = service.getCharacteristic(txUuid)
            rxCharacteristic = service.getCharacteristic(rxUuid)
            val tx = txCharacteristic
            if (tx == null || rxCharacteristic == null) {
                handler.post { listener.onStatus("TX/RX characteristics missing") }
                return
            }
            gatt.setCharacteristicNotification(tx, true)
            val cccd = tx.getDescriptor(cccdUuid)
            if (cccd != null) {
                cccd.value = BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE
                gatt.writeDescriptor(cccd)
            }
            handler.post { scheduleBackgroundJobs() }
        }

        @Deprecated("Deprecated in Java")
        override fun onCharacteristicChanged(
            gatt: BluetoothGatt,
            characteristic: BluetoothGattCharacteristic
        ) {
            handleNotify(characteristic.value ?: return)
        }

        override fun onCharacteristicChanged(
            gatt: BluetoothGatt,
            characteristic: BluetoothGattCharacteristic,
            value: ByteArray
        ) {
            handleNotify(value)
        }

        @SuppressLint("MissingPermission")
        override fun onCharacteristicWrite(
            gatt: BluetoothGatt,
            characteristic: BluetoothGattCharacteristic,
            status: Int
        ) {
            writeBusy = false
            pumpWriteQueue()
        }

        private fun handleNotify(value: ByteArray) {
            val frame = BleProtocol.decode(value) ?: return
            when (frame.type) {
                BleProtocol.TYPE_HELLO -> {
                    val version = frame.payload.toString(Charsets.UTF_8)
                    enqueueWrite(
                        BleProtocol.encode(
                            BleProtocol.TYPE_HELLO_ACK,
                            frame.reqId,
                            BleProtocol.APP_VERSION.toByteArray(Charsets.UTF_8)
                        )
                    )
                    handler.post {
                        listener.onHello(version)
                        listener.onStatus(
                            context.getString(R.string.hello_ok, version)
                        )
                        listener.onDetail("peer=${gatt?.device?.address ?: "?"}")
                    }
                }
                BleProtocol.TYPE_PONG -> {
                    // Keep-alive ack; quiet in UI to avoid noise while locked.
                }
                BleProtocol.TYPE_ERROR -> {
                    val code = frame.payload.firstOrNull()?.toInt()?.and(0xFF) ?: -1
                    handler.post { listener.onDetail("Error code=$code") }
                }
                BleProtocol.TYPE_HTTP_LIST_REQ -> handleListReq(frame.reqId)
                BleProtocol.TYPE_HTTP_RUN_REQ -> handleRunReq(frame)
                else -> Unit
            }
        }
    }

    private fun handleListReq(reqId: Int) {
        val presets = listener.presets().take(BleProtocol.HTTP_MAX_PRESETS)
        handler.post {
            listener.onDetail("HttpListReq → ${presets.size} preset(s)")
        }
        if (presets.isEmpty()) {
            enqueueWrite(BleProtocol.encode(BleProtocol.TYPE_HTTP_LIST_END, reqId))
            return
        }
        val total = presets.size
        presets.forEachIndexed { index, preset ->
            val nameBytes = preset.name.take(BleProtocol.HTTP_NAME_MAX)
                .toByteArray(Charsets.UTF_8)
            val maxName = BleProtocol.MAX_PAYLOAD - 2
            val name = if (nameBytes.size > maxName) nameBytes.copyOf(maxName) else nameBytes
            val payload = ByteArray(2 + name.size)
            payload[0] = index.toByte()
            payload[1] = total.toByte()
            System.arraycopy(name, 0, payload, 2, name.size)
            enqueueWrite(BleProtocol.encode(BleProtocol.TYPE_HTTP_LIST_ITEM, reqId, payload))
        }
        enqueueWrite(BleProtocol.encode(BleProtocol.TYPE_HTTP_LIST_END, reqId))
    }

    private fun handleRunReq(frame: BleProtocol.Frame) {
        if (frame.payload.isEmpty()) {
            enqueueWrite(
                BleProtocol.encode(
                    BleProtocol.TYPE_ERROR,
                    frame.reqId,
                    byteArrayOf(BleProtocol.ERR_BAD_FRAME.toByte())
                )
            )
            return
        }
        val index = frame.payload[0].toInt() and 0xFF
        val presets = listener.presets()
        if (index >= presets.size) {
            enqueueWrite(
                BleProtocol.encode(
                    BleProtocol.TYPE_ERROR,
                    frame.reqId,
                    byteArrayOf(BleProtocol.ERR_HTTP_FAILED.toByte())
                )
            )
            return
        }
        val preset = presets[index]
        handler.post { listener.onDetail("HttpRunReq → ${preset.name}") }
        HttpExecutor.executeAsync(preset) { result ->
            handler.post {
                sendRunResult(frame.reqId, result)
            }
        }
    }

    private fun sendRunResult(reqId: Int, result: HttpExecResult) {
        val meta = ByteArray(4)
        meta[0] = (result.httpStatus and 0xFF).toByte()
        meta[1] = ((result.httpStatus shr 8) and 0xFF).toByte()
        meta[2] = (result.errCode and 0xFF).toByte()
        meta[3] = 0
        enqueueWrite(BleProtocol.encode(BleProtocol.TYPE_HTTP_RUN_META, reqId, meta))

        var offset = 0
        var seq = 0
        val body = result.body
        while (offset < body.size) {
            val n = minOf(BleProtocol.HTTP_CHUNK_DATA_MAX, body.size - offset)
            val chunk = ByteArray(1 + n)
            chunk[0] = (seq and 0xFF).toByte()
            System.arraycopy(body, offset, chunk, 1, n)
            enqueueWrite(BleProtocol.encode(BleProtocol.TYPE_HTTP_RUN_CHUNK, reqId, chunk))
            offset += n
            seq++
        }
        enqueueWrite(BleProtocol.encode(BleProtocol.TYPE_HTTP_RUN_END, reqId))
        listener.onDetail(
            "HttpRun done status=${result.httpStatus} err=${result.errCode} bytes=${body.size}"
        )
    }

    private fun enqueueWrite(bytes: ByteArray) {
        writeQueue.addLast(bytes)
        pumpWriteQueue()
    }

    @SuppressLint("MissingPermission")
    private fun pumpWriteQueue() {
        if (writeBusy) return
        val characteristic = rxCharacteristic ?: return
        val g = gatt ?: return
        val next = writeQueue.pollFirst() ?: return
        writeBusy = true
        characteristic.value = next
        characteristic.writeType = BluetoothGattCharacteristic.WRITE_TYPE_DEFAULT
        if (!g.writeCharacteristic(characteristic)) {
            writeBusy = false
            handler.post { listener.onDetail("BLE write failed") }
        }
    }

    companion object {
        private const val PREFS = "ble_link"
        private const val KEY_LAST_ADDR = "last_peer_address"
        private const val RESUME_LISTEN_DELAY_MS = 2000L
        private const val SCAN_WATCHDOG_MS = 25_000L
        private const val SCAN_RETRY_AFTER_FAIL_MS = 5_000L
        private const val KEEP_ALIVE_MS = 25_000L
        /** Renewed while listening/connected; release on full stop. */
        private const val WAKE_LOCK_TIMEOUT_MS = 60 * 60 * 1000L
    }
}
