package com.cardputer.companion

import android.content.Context
import android.content.Intent
import java.util.concurrent.ConcurrentHashMap
import java.util.concurrent.CountDownLatch
import java.util.concurrent.TimeUnit
import java.util.concurrent.atomic.AtomicReference

/**
 * Shows trust / mismatch dialogs via [SshHostKeyPromptActivity] (works from a Service thread).
 */
class AndroidSshHostKeyTrustPrompter(private val context: Context) : SshHostKeyTrustPrompter {

    override fun confirmNewHost(host: String, port: Int, offered: SshHostKeyPin): Boolean {
        return SshHostKeyPromptActivity.awaitUserDecision(
            context = context,
            mode = SshHostKeyPromptActivity.MODE_TRUST,
            host = host,
            port = port,
            offered = offered,
            stored = null
        )
    }

    override fun onHostKeyMismatch(
        host: String,
        port: Int,
        stored: SshHostKeyPin,
        offered: SshHostKeyPin
    ) {
        SshHostKeyPromptActivity.awaitUserDecision(
            context = context,
            mode = SshHostKeyPromptActivity.MODE_MISMATCH,
            host = host,
            port = port,
            offered = offered,
            stored = stored
        )
    }
}

/**
 * Transparent activity: SSH host key trust or mismatch warning.
 */
class SshHostKeyPromptActivity : androidx.appcompat.app.AppCompatActivity() {

    override fun onCreate(savedInstanceState: android.os.Bundle?) {
        super.onCreate(savedInstanceState)
        val requestId = intent.getStringExtra(EXTRA_REQUEST_ID) ?: run {
            finish()
            return
        }
        val pending = PendingPrompts.take(requestId) ?: run {
            finish()
            return
        }
        val mode = intent.getStringExtra(EXTRA_MODE)
        val host = intent.getStringExtra(EXTRA_HOST).orEmpty()
        val port = intent.getIntExtra(EXTRA_PORT, 22)
        val offeredType = intent.getStringExtra(EXTRA_OFFERED_TYPE).orEmpty()
        val offeredFp = intent.getStringExtra(EXTRA_OFFERED_FP).orEmpty()
        val storedType = intent.getStringExtra(EXTRA_STORED_TYPE)
        val storedFp = intent.getStringExtra(EXTRA_STORED_FP)

        when (mode) {
            MODE_TRUST -> {
                val message = getString(
                    R.string.ssh_hostkey_trust_message,
                    host,
                    port,
                    offeredType,
                    offeredFp
                )
                androidx.appcompat.app.AlertDialog.Builder(this)
                    .setTitle(R.string.ssh_hostkey_trust_title)
                    .setMessage(message)
                    .setCancelable(false)
                    .setPositiveButton(R.string.ssh_hostkey_trust_accept) { _, _ ->
                        pending.complete(true)
                        finish()
                    }
                    .setNegativeButton(R.string.cancel) { _, _ ->
                        pending.complete(false)
                        finish()
                    }
                    .setOnDismissListener {
                        if (!pending.isDone()) pending.complete(false)
                    }
                    .show()
            }
            MODE_MISMATCH -> {
                val message = getString(
                    R.string.ssh_hostkey_mismatch_message,
                    host,
                    port,
                    storedType ?: "?",
                    storedFp ?: "?",
                    offeredType,
                    offeredFp
                )
                androidx.appcompat.app.AlertDialog.Builder(this)
                    .setTitle(R.string.ssh_hostkey_mismatch_title)
                    .setMessage(message)
                    .setCancelable(false)
                    .setPositiveButton(R.string.ok) { _, _ ->
                        pending.complete(false)
                        finish()
                    }
                    .setOnDismissListener {
                        if (!pending.isDone()) pending.complete(false)
                    }
                    .show()
            }
            else -> {
                pending.complete(false)
                finish()
            }
        }
    }

    companion object {
        const val MODE_TRUST = "trust"
        const val MODE_MISMATCH = "mismatch"

        private const val EXTRA_REQUEST_ID = "request_id"
        private const val EXTRA_MODE = "mode"
        private const val EXTRA_HOST = "host"
        private const val EXTRA_PORT = "port"
        private const val EXTRA_OFFERED_TYPE = "offered_type"
        private const val EXTRA_OFFERED_FP = "offered_fp"
        private const val EXTRA_STORED_TYPE = "stored_type"
        private const val EXTRA_STORED_FP = "stored_fp"

        private const val PROMPT_TIMEOUT_SEC = 120L

        fun awaitUserDecision(
            context: Context,
            mode: String,
            host: String,
            port: Int,
            offered: SshHostKeyPin,
            stored: SshHostKeyPin?
        ): Boolean {
            val requestId = java.util.UUID.randomUUID().toString()
            val pending = PendingPrompts.register(requestId)
            val intent = Intent(context, SshHostKeyPromptActivity::class.java).apply {
                addFlags(Intent.FLAG_ACTIVITY_NEW_TASK)
                putExtra(EXTRA_REQUEST_ID, requestId)
                putExtra(EXTRA_MODE, mode)
                putExtra(EXTRA_HOST, host)
                putExtra(EXTRA_PORT, port)
                putExtra(EXTRA_OFFERED_TYPE, offered.keyType)
                putExtra(EXTRA_OFFERED_FP, offered.displayFingerprint())
                putExtra(EXTRA_STORED_TYPE, stored?.keyType)
                putExtra(EXTRA_STORED_FP, stored?.displayFingerprint())
            }
            context.startActivity(intent)
            return pending.await(PROMPT_TIMEOUT_SEC)
        }
    }
}

private object PendingPrompts {
    private val pending = ConcurrentHashMap<String, Entry>()

    fun register(id: String): Entry {
        val entry = Entry()
        pending[id] = entry
        return entry
    }

    fun take(id: String): Entry? = pending.remove(id)

    class Entry {
        private val latch = CountDownLatch(1)
        private val result = AtomicReference<Boolean?>(null)

        fun complete(accepted: Boolean) {
            result.set(accepted)
            latch.countDown()
        }

        fun isDone(): Boolean = result.get() != null

        fun await(timeoutSec: Long): Boolean {
            latch.await(timeoutSec, TimeUnit.SECONDS)
            return result.get() == true
        }
    }
}
