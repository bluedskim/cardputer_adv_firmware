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
    val body: ByteArray,
    val log: String = ""
) {
    override fun equals(other: Any?): Boolean {
        if (this === other) return true
        if (javaClass != other?.javaClass) return false
        other as HttpExecResult
        if (httpStatus != other.httpStatus) return false
        if (errCode != other.errCode) return false
        if (!body.contentEquals(other.body)) return false
        if (log != other.log) return false
        return true
    }

    override fun hashCode(): Int {
        var result = httpStatus
        result = 31 * result + errCode
        result = 31 * result + body.contentHashCode()
        result = 31 * result + log.hashCode()
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
        val log = StringBuilder()
        fun line(msg: String) {
            log.append(msg).append('\n')
        }
        var conn: HttpURLConnection? = null
        return try {
            val method = preset.method.uppercase().ifBlank { "GET" }
            line("HTTP $method ${preset.url}")
            val url = URL(preset.url)
            conn = (url.openConnection() as HttpURLConnection).apply {
                requestMethod = method
                connectTimeout = 10_000
                readTimeout = 15_000
                doInput = true
                instanceFollowRedirects = true
                setRequestProperty("User-Agent", "CardputerCompanion/${BleProtocol.APP_VERSION}")
            }
            var headerCount = 0
            for (headerLine in preset.headers.lineSequence()) {
                val trimmed = headerLine.trim()
                if (trimmed.isEmpty()) continue
                val colon = trimmed.indexOf(':')
                if (colon <= 0) continue
                val key = trimmed.substring(0, colon).trim()
                val value = trimmed.substring(colon + 1).trim()
                if (key.isNotEmpty()) {
                    conn.setRequestProperty(key, value)
                    headerCount++
                }
            }
            line("headers=$headerCount connectTimeout=10s readTimeout=15s")
            if (method == "POST" || method == "PUT" || method == "PATCH") {
                conn.doOutput = true
                line("writing body ${preset.body.length} chars")
                OutputStreamWriter(conn.outputStream, StandardCharsets.UTF_8).use { w ->
                    w.write(preset.body)
                }
            }
            line("connecting…")
            val status = conn.responseCode
            line("response status=$status")
            val stream = if (status in 200..299) conn.inputStream else conn.errorStream
            val text = if (stream != null) {
                BufferedReader(InputStreamReader(stream, StandardCharsets.UTF_8)).use { it.readText() }
            } else {
                ""
            }
            val fullLen = text.toByteArray(StandardCharsets.UTF_8).size
            val bytes = text.toByteArray(StandardCharsets.UTF_8)
                .take(BleProtocol.HTTP_BODY_MAX)
                .toByteArray()
            if (fullLen > BleProtocol.HTTP_BODY_MAX) {
                line("body truncated $fullLen → ${BleProtocol.HTTP_BODY_MAX} bytes")
            } else {
                line("body ${bytes.size} bytes")
            }
            line("ok")
            HttpExecResult(status, 0, bytes, log.toString().trimEnd())
        } catch (e: java.net.SocketTimeoutException) {
            line("timeout: ${e.message ?: e.javaClass.simpleName}")
            HttpExecResult(0, BleProtocol.ERR_HTTP_TIMEOUT, ByteArray(0), log.toString().trimEnd())
        } catch (e: Exception) {
            line("error ${e.javaClass.simpleName}: ${e.message ?: "(no message)"}")
            HttpExecResult(0, BleProtocol.ERR_HTTP_FAILED, ByteArray(0), log.toString().trimEnd())
        } finally {
            conn?.disconnect()
        }
    }
}
