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
import android.os.Handler
import android.os.Looper
import java.util.ArrayDeque
import java.util.UUID

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

    private var gatt: BluetoothGatt? = null
    private var txCharacteristic: BluetoothGattCharacteristic? = null
    private var rxCharacteristic: BluetoothGattCharacteristic? = null
    private var scanning = false
    private var writeBusy = false
    private val writeQueue: ArrayDeque<ByteArray> = ArrayDeque()

    private val serviceUuid = UUID.fromString(BleProtocol.SERVICE_UUID)
    private val rxUuid = UUID.fromString(BleProtocol.RX_UUID)
    private val txUuid = UUID.fromString(BleProtocol.TX_UUID)
    private val cccdUuid = UUID.fromString("00002902-0000-1000-8000-00805f9b34fb")

    fun isBluetoothEnabled(): Boolean = adapter?.isEnabled == true

    @SuppressLint("MissingPermission")
    fun startScan() {
        val bt = adapter
        if (bt == null || !bt.isEnabled) {
            listener.onStatus(context.getString(R.string.bt_off))
            return
        }
        stopScan()
        disconnect()
        scanning = true
        listener.onStatus(context.getString(R.string.scanning))
        listener.onDetail("")

        val filter = ScanFilter.Builder()
            .setDeviceName(BleProtocol.DEVICE_NAME)
            .build()
        val settings = ScanSettings.Builder()
            .setScanMode(ScanSettings.SCAN_MODE_LOW_LATENCY)
            .build()

        bt.bluetoothLeScanner?.startScan(listOf(filter), settings, scanCallback)
        handler.postDelayed({
            if (scanning) {
                stopScan()
                listener.onStatus(context.getString(R.string.no_device))
            }
        }, 12_000)
    }

    @SuppressLint("MissingPermission")
    fun stopScan() {
        if (!scanning) return
        scanning = false
        adapter?.bluetoothLeScanner?.stopScan(scanCallback)
    }

    @SuppressLint("MissingPermission")
    fun disconnect() {
        stopScan()
        writeQueue.clear()
        writeBusy = false
        gatt?.disconnect()
        gatt?.close()
        gatt = null
        txCharacteristic = null
        rxCharacteristic = null
    }

    @SuppressLint("MissingPermission")
    private fun connect(device: BluetoothDevice) {
        stopScan()
        listener.onStatus(context.getString(R.string.connecting))
        gatt = device.connectGatt(context, false, gattCallback, BluetoothDevice.TRANSPORT_LE)
    }

    private val scanCallback = object : ScanCallback() {
        @SuppressLint("MissingPermission")
        override fun onScanResult(callbackType: Int, result: ScanResult) {
            val name = result.device.name ?: result.scanRecord?.deviceName
            if (name != BleProtocol.DEVICE_NAME) return
            stopScan()
            connect(result.device)
        }

        override fun onScanFailed(errorCode: Int) {
            scanning = false
            listener.onStatus("Scan failed: $errorCode")
        }
    }

    private val gattCallback = object : BluetoothGattCallback() {
        @SuppressLint("MissingPermission")
        override fun onConnectionStateChange(gatt: BluetoothGatt, status: Int, newState: Int) {
            if (newState == BluetoothProfile.STATE_CONNECTED) {
                handler.post {
                    listener.onStatus(context.getString(R.string.connected_waiting_hello))
                }
                gatt.discoverServices()
            } else if (newState == BluetoothProfile.STATE_DISCONNECTED) {
                handler.post {
                    listener.onStatus(context.getString(R.string.disconnected))
                    listener.onDisconnected()
                }
                writeQueue.clear()
                writeBusy = false
                gatt.close()
                if (this@BleLinkClient.gatt === gatt) {
                    this@BleLinkClient.gatt = null
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
                    handler.post { listener.onDetail("Pong req=${frame.reqId}") }
                }
                BleProtocol.TYPE_ERROR -> {
                    val code = frame.payload.firstOrNull()?.toInt()?.and(0xFF) ?: -1
                    handler.post { listener.onDetail("Error code=$code") }
                }
                BleProtocol.TYPE_HTTP_LIST_REQ -> handleListReq(frame.reqId)
                BleProtocol.TYPE_HTTP_RUN_REQ -> handleRunReq(frame)
                else -> {
                    if (BleProtocol.isHttpType(frame.type)) {
                        // ignore unexpected phone-bound HTTP responses echoed
                    }
                }
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
        listener.onDetail("HttpRun done status=${result.httpStatus} err=${result.errCode} bytes=${body.size}")
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
}
