package com.cardputer.companion

import net.schmizz.sshj.SSHClient
import net.schmizz.sshj.common.IOUtils
import net.schmizz.sshj.connection.channel.direct.Session
import net.schmizz.sshj.transport.verification.HostKeyVerifier
import net.schmizz.sshj.userauth.UserAuthException
import java.security.MessageDigest
import java.security.PublicKey
import java.util.concurrent.Executors
import java.util.concurrent.TimeUnit

data class SshExecResult(
    val exitStatus: Int,
    val errCode: Int,
    val body: ByteArray
) {
    override fun equals(other: Any?): Boolean {
        if (this === other) return true
        if (javaClass != other?.javaClass) return false
        other as SshExecResult
        if (exitStatus != other.exitStatus) return false
        if (errCode != other.errCode) return false
        if (!body.contentEquals(other.body)) return false
        return true
    }

    override fun hashCode(): Int {
        var result = exitStatus
        result = 31 * result + errCode
        result = 31 * result + body.contentHashCode()
        return result
    }
}

object SshExecutor {
    private val executor = Executors.newSingleThreadExecutor()

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
        val client = SSHClient()
        client.timeout = 15_000
        client.connectTimeout = 10_000
        return try {
            client.addHostKeyVerifier(TofuVerifier(preset.host, preset.port, hostKeys))
            client.connect(preset.host, preset.port)
            client.authPassword(preset.user, preset.password)
            client.startSession().use { session ->
                val cmd: Session.Command = session.exec(preset.command)
                val stdout = IOUtils.readFully(cmd.inputStream).toByteArray()
                val stderr = IOUtils.readFully(cmd.errorStream).toByteArray()
                cmd.join(20, TimeUnit.SECONDS)
                val exit = cmd.exitStatus ?: -1
                val combined = (stdout + stderr)
                    .take(BleProtocol.SSH_BODY_MAX)
                    .toByteArray()
                SshExecResult(exit, 0, combined)
            }
        } catch (_: UserAuthException) {
            SshExecResult(0, BleProtocol.ERR_SSH_AUTH_FAILED, ByteArray(0))
        } catch (_: java.net.SocketTimeoutException) {
            SshExecResult(0, BleProtocol.ERR_SSH_TIMEOUT, ByteArray(0))
        } catch (_: java.util.concurrent.TimeoutException) {
            SshExecResult(0, BleProtocol.ERR_SSH_TIMEOUT, ByteArray(0))
        } catch (e: Exception) {
            val msg = e.message.orEmpty().lowercase()
            when {
                msg.contains("host key") || msg.contains("could not verify") ->
                    SshExecResult(0, BleProtocol.ERR_SSH_HOST_KEY, ByteArray(0))
                msg.contains("auth") || msg.contains("password") ->
                    SshExecResult(0, BleProtocol.ERR_SSH_AUTH_FAILED, ByteArray(0))
                msg.contains("timeout") || msg.contains("timed out") ->
                    SshExecResult(0, BleProtocol.ERR_SSH_TIMEOUT, ByteArray(0))
                else ->
                    SshExecResult(0, BleProtocol.ERR_SSH_FAILED, ByteArray(0))
            }
        } finally {
            try {
                client.disconnect()
            } catch (_: Exception) {
            }
        }
    }

    private class TofuVerifier(
        private val host: String,
        private val port: Int,
        private val store: SshHostKeyStore
    ) : HostKeyVerifier {
        override fun verify(hostname: String?, port: Int, key: PublicKey?): Boolean {
            if (key == null) return false
            val fp = fingerprint(key)
            val stored = store.getFingerprint(host, this.port)
                ?: store.getFingerprint(hostname ?: host, port)
            return when {
                stored == null -> {
                    store.putFingerprint(host, this.port, fp)
                    true
                }
                stored.equals(fp, ignoreCase = true) -> true
                else -> false
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
