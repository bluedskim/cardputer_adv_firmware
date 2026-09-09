package com.cardputer.companion

import java.io.BufferedReader
import java.io.InputStreamReader
import java.io.OutputStreamWriter
import java.net.HttpURLConnection
import java.net.URL
import java.nio.charset.StandardCharsets
import java.util.concurrent.Executors

data class HttpExecResult(
    val httpStatus: Int,
    val errCode: Int,
    val body: ByteArray
) {
    override fun equals(other: Any?): Boolean {
        if (this === other) return true
        if (javaClass != other?.javaClass) return false
        other as HttpExecResult
        if (httpStatus != other.httpStatus) return false
        if (errCode != other.errCode) return false
        if (!body.contentEquals(other.body)) return false
        return true
    }

    override fun hashCode(): Int {
        var result = httpStatus
        result = 31 * result + errCode
        result = 31 * result + body.contentHashCode()
        return result
    }
}

object HttpExecutor {
    private val executor = Executors.newSingleThreadExecutor()

    fun executeAsync(preset: HttpPreset, callback: (HttpExecResult) -> Unit) {
        executor.execute {
            callback(execute(preset))
        }
    }

    fun execute(preset: HttpPreset): HttpExecResult {
        var conn: HttpURLConnection? = null
        return try {
            val url = URL(preset.url)
            conn = (url.openConnection() as HttpURLConnection).apply {
                requestMethod = preset.method.uppercase().ifBlank { "GET" }
                connectTimeout = 10_000
                readTimeout = 15_000
                doInput = true
                instanceFollowRedirects = true
                setRequestProperty("User-Agent", "CardputerCompanion/${BleProtocol.APP_VERSION}")
            }
            for (line in preset.headers.lineSequence()) {
                val trimmed = line.trim()
                if (trimmed.isEmpty()) continue
                val colon = trimmed.indexOf(':')
                if (colon <= 0) continue
                val key = trimmed.substring(0, colon).trim()
                val value = trimmed.substring(colon + 1).trim()
                if (key.isNotEmpty()) {
                    conn.setRequestProperty(key, value)
                }
            }
            val method = conn.requestMethod
            if (method == "POST" || method == "PUT" || method == "PATCH") {
                conn.doOutput = true
                OutputStreamWriter(conn.outputStream, StandardCharsets.UTF_8).use { w ->
                    w.write(preset.body)
                }
            }
            val status = conn.responseCode
            val stream = if (status in 200..299) conn.inputStream else conn.errorStream
            val text = if (stream != null) {
                BufferedReader(InputStreamReader(stream, StandardCharsets.UTF_8)).use { it.readText() }
            } else {
                ""
            }
            val bytes = text.toByteArray(StandardCharsets.UTF_8)
                .take(BleProtocol.HTTP_BODY_MAX)
                .toByteArray()
            HttpExecResult(status, 0, bytes)
        } catch (_: java.net.SocketTimeoutException) {
            HttpExecResult(0, BleProtocol.ERR_HTTP_TIMEOUT, ByteArray(0))
        } catch (_: Exception) {
            HttpExecResult(0, BleProtocol.ERR_HTTP_FAILED, ByteArray(0))
        } finally {
            conn?.disconnect()
        }
    }
}
