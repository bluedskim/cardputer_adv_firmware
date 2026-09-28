package com.cardputer.companion

/**
 * Keep in sync with include/ble/ble_protocol.h and docs/ble_link.md
 */
object BleProtocol {
    const val DEVICE_NAME = "CardputerAdv"
    const val SERVICE_UUID = "c7a1f001-5b2e-4a1d-9c3f-0d1e2f3a4b5c"
    const val RX_UUID = "c7a1f002-5b2e-4a1d-9c3f-0d1e2f3a4b5c"
    const val TX_UUID = "c7a1f003-5b2e-4a1d-9c3f-0d1e2f3a4b5c"

    const val HEADER_SIZE = 5
    const val MAX_PAYLOAD = 48
    const val HTTP_BODY_MAX = 512
    const val HTTP_CHUNK_DATA_MAX = MAX_PAYLOAD - 1 // seq byte + data
    const val HTTP_NAME_MAX = 20
    const val HTTP_MAX_PRESETS = 12

    const val TYPE_HELLO: Byte = 0x01
    const val TYPE_HELLO_ACK: Byte = 0x02
    @Suppress("unused")
    const val TYPE_PING: Byte = 0x03
    const val TYPE_PONG: Byte = 0x04

    const val TYPE_HTTP_LIST_REQ: Byte = 0x10
    const val TYPE_HTTP_LIST_ITEM: Byte = 0x11
    const val TYPE_HTTP_LIST_END: Byte = 0x12
    const val TYPE_HTTP_RUN_REQ: Byte = 0x13
    const val TYPE_HTTP_RUN_META: Byte = 0x14
    const val TYPE_HTTP_RUN_CHUNK: Byte = 0x15
    const val TYPE_HTTP_RUN_END: Byte = 0x16

    const val TYPE_SSH_LIST_REQ: Byte = 0x20
    const val TYPE_SSH_LIST_ITEM: Byte = 0x21
    const val TYPE_SSH_LIST_END: Byte = 0x22
    const val TYPE_SSH_RUN_REQ: Byte = 0x23
    const val TYPE_SSH_RUN_META: Byte = 0x24
    const val TYPE_SSH_RUN_CHUNK: Byte = 0x25
    const val TYPE_SSH_RUN_END: Byte = 0x26

    const val TYPE_FIND_START: Byte = 0x40
    const val TYPE_FIND_STARTED: Byte = 0x41
    const val TYPE_FIND_STOP: Byte = 0x42
    const val TYPE_FIND_STOPPED: Byte = 0x43
    const val FIND_FLAG_SOUND: Int = 0x01
    const val FIND_REASON_USER: Int = 0
    const val FIND_REASON_TIMEOUT: Int = 1
    const val FIND_REASON_ERROR: Int = 2

    const val TYPE_ERROR: Byte = 0x7F

    const val ERR_BAD_FRAME: Int = 0x01
    const val ERR_UNKNOWN_TYPE: Int = 0x02
    const val ERR_NOT_READY: Int = 0x03
    const val ERR_HTTP_FAILED: Int = 0x10
    const val ERR_HTTP_TIMEOUT: Int = 0x11
    const val ERR_BLE_NOT_READY: Int = 0x12
    const val ERR_NO_PRESETS: Int = 0x13
    const val ERR_SSH_FAILED: Int = 0x20
    const val ERR_SSH_TIMEOUT: Int = 0x21
    const val ERR_SSH_AUTH_FAILED: Int = 0x22
    const val ERR_SSH_HOST_KEY: Int = 0x23
    const val ERR_NO_SSH_PRESETS: Int = 0x24
    const val ERR_FIND_FAILED: Int = 0x40

    const val SSH_BODY_MAX = 512
    const val SSH_CHUNK_DATA_MAX = MAX_PAYLOAD - 1
    const val SSH_NAME_MAX = 20
    const val SSH_MAX_PRESETS = 12

    const val APP_VERSION = "0.8.1"

    data class Frame(
        val type: Byte,
        val reqId: Int,
        val payload: ByteArray
    ) {
        override fun equals(other: Any?): Boolean {
            if (this === other) return true
            if (javaClass != other?.javaClass) return false
            other as Frame
            if (type != other.type) return false
            if (reqId != other.reqId) return false
            if (!payload.contentEquals(other.payload)) return false
            return true
        }

        override fun hashCode(): Int {
            var result = type.hashCode()
            result = 31 * result + reqId
            result = 31 * result + payload.contentHashCode()
            return result
        }
    }

    fun encode(type: Byte, reqId: Int, payload: ByteArray = ByteArray(0)): ByteArray {
        require(payload.size <= MAX_PAYLOAD)
        val out = ByteArray(HEADER_SIZE + payload.size)
        out[0] = type
        out[1] = (reqId and 0xFF).toByte()
        out[2] = ((reqId shr 8) and 0xFF).toByte()
        out[3] = (payload.size and 0xFF).toByte()
        out[4] = 0
        if (payload.isNotEmpty()) {
            System.arraycopy(payload, 0, out, HEADER_SIZE, payload.size)
        }
        return out
    }

    fun decode(data: ByteArray): Frame? {
        if (data.size < HEADER_SIZE) return null
        val type = data[0]
        val reqId = (data[1].toInt() and 0xFF) or ((data[2].toInt() and 0xFF) shl 8)
        val len = (data[3].toInt() and 0xFF) or ((data[4].toInt() and 0xFF) shl 8)
        if (len > MAX_PAYLOAD || data.size < HEADER_SIZE + len) return null
        val payload = if (len == 0) ByteArray(0) else data.copyOfRange(HEADER_SIZE, HEADER_SIZE + len)
        return Frame(type, reqId, payload)
    }

    fun isHttpType(type: Byte): Boolean {
        val v = type.toInt() and 0xFF
        return v in 0x10..0x1F
    }

    fun isSshType(type: Byte): Boolean {
        val v = type.toInt() and 0xFF
        return v in 0x20..0x2F
    }

    fun isFindType(type: Byte): Boolean {
        val v = type.toInt() and 0xFF
        return v in 0x40..0x4F
    }
}
