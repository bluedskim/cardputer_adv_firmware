package com.cardputer.companion

import net.schmizz.sshj.DefaultConfig
import net.schmizz.sshj.SSHClient
import net.schmizz.sshj.common.IOUtils
import net.schmizz.sshj.common.SecurityUtils
import net.schmizz.sshj.connection.channel.direct.Session
import net.schmizz.sshj.transport.verification.HostKeyVerifier
import net.schmizz.sshj.userauth.UserAuthException
import org.bouncycastle.jce.provider.BouncyCastleProvider
import android.util.Log
import java.security.MessageDigest
import java.security.PublicKey
import java.security.Security
import java.util.concurrent.Executors
import java.util.concurrent.TimeUnit
import java.util.concurrent.atomic.AtomicBoolean

data class SshExecResult(
    val exitStatus: Int,
    val errCode: Int,
    val body: ByteArray,
    val log: String = ""
) {
    override fun equals(other: Any?): Boolean {
        if (this === other) return true
        if (javaClass != other?.javaClass) return false
        other as SshExecResult
        if (exitStatus != other.exitStatus) return false
        if (errCode != other.errCode) return false
        if (!body.contentEquals(other.body)) return false
        if (log != other.log) return false
        return true
    }

    override fun hashCode(): Int {
        var result = exitStatus
        result = 31 * result + errCode
        result = 31 * result + body.contentHashCode()
        result = 31 * result + log.hashCode()
        return result
    }
}

object SshExecutor {
    private const val TAG = "SshExecutor"
    private val executor = Executors.newSingleThreadExecutor()
    private val bcReady = AtomicBoolean(false)

    fun executeAsync(
        preset: SshPreset,
        hostKeys: SshHostKeyStore,
        callback: (SshExecResult) -> Unit
    ) {
        executor.execute {
            callback(execute(preset, hostKeys))
        }
    }

    fun execute(preset: SshPreset, hostKeys: SshHostKeyStore): SshExecResult {
        val log = StringBuilder()
        fun line(msg: String) {
            log.append(msg).append('\n')
            Log.i(TAG, msg)
        }
        ensureBouncyCastle(::line)
        val client = SSHClient(DefaultConfig())
        client.timeout = 15_000
        client.connectTimeout = 10_000
        return try {
            line("SSH ${preset.user}@${preset.host}:${preset.port}")
            line("command: ${preset.command}")
            line("password: ${if (preset.password.isEmpty()) "(empty)" else "(set)"}")
            client.addHostKeyVerifier(TofuVerifier(preset.host, preset.port, hostKeys, ::line))
            line("connecting…")
            client.connect(preset.host, preset.port)
            line("connected, authenticating…")
            client.authPassword(preset.user, preset.password)
            line("auth ok, exec…")
            client.startSession().use { session ->
                val cmd: Session.Command = session.exec(preset.command)
                val stdout = IOUtils.readFully(cmd.inputStream).toByteArray()
                val stderr = IOUtils.readFully(cmd.errorStream).toByteArray()
                cmd.join(20, TimeUnit.SECONDS)
                val exit = cmd.exitStatus ?: -1
                line("exit=$exit stdout=${stdout.size}B stderr=${stderr.size}B")
                val combinedFull = stdout + stderr
                val combined = combinedFull.take(BleProtocol.SSH_BODY_MAX).toByteArray()
                if (combinedFull.size > BleProtocol.SSH_BODY_MAX) {
                    line("body truncated ${combinedFull.size} → ${BleProtocol.SSH_BODY_MAX} bytes")
                }
                line("ok")
                SshExecResult(exit, 0, combined, log.toString().trimEnd())
            }
        } catch (e: UserAuthException) {
            line("auth failed: ${e.message ?: e.javaClass.simpleName}")
            SshExecResult(0, BleProtocol.ERR_SSH_AUTH_FAILED, ByteArray(0), log.toString().trimEnd())
        } catch (e: java.net.SocketTimeoutException) {
            line("timeout: ${e.message ?: e.javaClass.simpleName}")
            SshExecResult(0, BleProtocol.ERR_SSH_TIMEOUT, ByteArray(0), log.toString().trimEnd())
        } catch (e: java.util.concurrent.TimeoutException) {
            line("timeout: ${e.message ?: e.javaClass.simpleName}")
            SshExecResult(0, BleProtocol.ERR_SSH_TIMEOUT, ByteArray(0), log.toString().trimEnd())
        } catch (e: Exception) {
            line("error ${e.javaClass.simpleName}: ${e.message ?: "(no message)"}")
            val msg = e.message.orEmpty().lowercase()
            val code = when {
                msg.contains("host key") || msg.contains("could not verify") ->
                    BleProtocol.ERR_SSH_HOST_KEY
                msg.contains("auth") || msg.contains("password") ->
                    BleProtocol.ERR_SSH_AUTH_FAILED
                msg.contains("timeout") || msg.contains("timed out") ->
                    BleProtocol.ERR_SSH_TIMEOUT
                else ->
                    BleProtocol.ERR_SSH_FAILED
            }
            SshExecResult(0, code, ByteArray(0), log.toString().trimEnd())
        } finally {
            try {
                client.disconnect()
            } catch (_: Exception) {
            }
        }
    }

    /**
     * Android ships an incomplete BouncyCastle as provider "BC". sshj looks up
     * X25519 via that name and fails. Replace it with the app-bundled provider.
     */
    private fun ensureBouncyCastle(logLine: (String) -> Unit) {
        if (bcReady.get()) {
            logLine("crypto: BC ready")
            return
        }
        synchronized(bcReady) {
            if (bcReady.get()) {
                logLine("crypto: BC ready")
                return
            }
            val existing = Security.getProvider(BouncyCastleProvider.PROVIDER_NAME)
            if (existing != null) {
                logLine("crypto: replacing Android BC (${existing.javaClass.name})")
                Security.removeProvider(BouncyCastleProvider.PROVIDER_NAME)
            } else {
                logLine("crypto: inserting BouncyCastle provider")
            }
            Security.insertProviderAt(BouncyCastleProvider(), 1)
            SecurityUtils.setRegisterBouncyCastle(true)
            try {
                javax.crypto.KeyAgreement.getInstance("X25519", BouncyCastleProvider.PROVIDER_NAME)
                logLine("crypto: X25519 available via BC")
            } catch (e: Exception) {
                logLine("crypto: X25519 still missing (${e.message}) — Curve25519 KEX may fail")
            }
            bcReady.set(true)
        }
    }

    private class TofuVerifier(
        private val host: String,
        private val port: Int,
        private val store: SshHostKeyStore,
        private val logLine: (String) -> Unit
    ) : HostKeyVerifier {
        override fun verify(hostname: String?, port: Int, key: PublicKey?): Boolean {
            if (key == null) {
                logLine("host key missing")
                return false
            }
            val fp = fingerprint(key)
            val shortFp = if (fp.length > 16) fp.take(16) + "…" else fp
            val stored = store.getFingerprint(host, this.port)
                ?: store.getFingerprint(hostname ?: host, port)
            return when {
                stored == null -> {
                    store.putFingerprint(host, this.port, fp)
                    logLine("host key TOFU accept sha256=$shortFp")
                    true
                }
                stored.equals(fp, ignoreCase = true) -> {
                    logLine("host key ok sha256=$shortFp")
                    true
                }
                else -> {
                    logLine("host key MISMATCH sha256=$shortFp")
                    false
                }
            }
        }

        override fun findExistingAlgorithms(hostname: String?, port: Int): MutableList<String> =
            mutableListOf()
    }

    private fun fingerprint(key: PublicKey): String {
        val digest = MessageDigest.getInstance("SHA-256").digest(key.encoded)
        return digest.joinToString("") { b -> "%02x".format(b) }
    }
}
