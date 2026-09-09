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

    const val TYPE_HELLO: Byte = 0x01
    const val TYPE_HELLO_ACK: Byte = 0x02
    @Suppress("unused") // protocol constant; phone→device Ping not sent yet
    const val TYPE_PING: Byte = 0x03
    const val TYPE_PONG: Byte = 0x04
    const val TYPE_ERROR: Byte = 0x7F

    const val APP_VERSION = "0.1.0-m1"

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
        out[4] = 0 // u16 LE high byte; always 0 while MAX_PAYLOAD <= 255
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
}
