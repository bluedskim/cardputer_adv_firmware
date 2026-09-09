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
import android.os.ParcelUuid
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
    }

    private val handler = Handler(Looper.getMainLooper())
    private val bluetoothManager =
        context.getSystemService(Context.BLUETOOTH_SERVICE) as BluetoothManager
    private val adapter: BluetoothAdapter? = bluetoothManager.adapter

    private var gatt: BluetoothGatt? = null
    private var txCharacteristic: BluetoothGattCharacteristic? = null
    private var rxCharacteristic: BluetoothGattCharacteristic? = null
    private var scanning = false

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
        private fun handleNotify(value: ByteArray) {
            val frame = BleProtocol.decode(value) ?: return
            when (frame.type) {
                BleProtocol.TYPE_HELLO -> {
                    val version = frame.payload.toString(Charsets.UTF_8)
                    val ack = BleProtocol.encode(
                        BleProtocol.TYPE_HELLO_ACK,
                        frame.reqId,
                        BleProtocol.APP_VERSION.toByteArray(Charsets.UTF_8)
                    )
                    writeRx(ack)
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
            }
        }
    }

    @SuppressLint("MissingPermission")
    private fun writeRx(bytes: ByteArray) {
        val characteristic = rxCharacteristic ?: return
        val g = gatt ?: return
        characteristic.value = bytes
        characteristic.writeType = BluetoothGattCharacteristic.WRITE_TYPE_DEFAULT
        g.writeCharacteristic(characteristic)
    }
}
