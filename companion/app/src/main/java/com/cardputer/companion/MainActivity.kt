package com.cardputer.companion

import android.Manifest
import android.app.Activity
import android.bluetooth.BluetoothAdapter
import android.content.ComponentName
import android.content.Context
import android.content.Intent
import android.content.IntentSender
import android.content.ServiceConnection
import android.content.pm.PackageManager
import android.net.Uri
import android.os.Build
import android.os.Bundle
import android.os.IBinder
import android.os.PowerManager
import android.provider.Settings
import android.view.LayoutInflater
import android.widget.ArrayAdapter
import android.widget.ScrollView
import android.widget.TextView
import android.widget.Toast
import androidx.activity.result.IntentSenderRequest
import androidx.activity.result.contract.ActivityResultContracts
import androidx.appcompat.app.AlertDialog
import androidx.appcompat.app.AppCompatActivity
import androidx.core.content.ContextCompat
import com.cardputer.companion.databinding.ActivityMainBinding
import com.cardputer.companion.databinding.DialogPresetBinding
import com.cardputer.companion.databinding.DialogSshPresetBinding
import java.nio.charset.StandardCharsets
import java.util.UUID
import java.util.concurrent.Executor

class MainActivity : AppCompatActivity(), BleLinkClient.Listener {
    private lateinit var binding: ActivityMainBinding
    private lateinit var store: PresetStore
    private lateinit var sshStore: SshPresetStore
    private lateinit var sshHostKeys: SshHostKeyStore
    private val presets = mutableListOf<HttpPreset>()
    private val sshPresets = mutableListOf<SshPreset>()
    private lateinit var listAdapter: ArrayAdapter<String>
    private lateinit var sshListAdapter: ArrayAdapter<String>
    private var resumeListenAfterBtEnable = false
    private var forceRescanPending = false
    private var associateAfterBt = false
    private var testRunInFlight = false

    private var bleService: BleLinkService? = null
    private var bound = false
    private val mainExecutor: Executor = Executor { it.run() }

    private val serviceConnection = object : ServiceConnection {
        override fun onServiceConnected(name: ComponentName?, binder: IBinder?) {
            val service = (binder as BleLinkService.LocalBinder).getService()
            bleService = service
            bound = true
            service.addUiListener(this@MainActivity)
            if (forceRescanPending) {
                forceRescanPending = false
                service.client.startScan()
            } else if (!service.client.isConnected()) {
                service.client.startListening(forceRescan = false)
            }
        }

        override fun onServiceDisconnected(name: ComponentName?) {
            bleService?.removeUiListener(this@MainActivity)
            bleService = null
            bound = false
        }
    }

    private val permissionLauncher =
        registerForActivityResult(ActivityResultContracts.RequestMultiplePermissions()) { result ->
            val granted = result.values.all { it }
            if (granted) {
                if (associateAfterBt) {
                    associateAfterBt = false
                    ensureBtAndAssociate()
                } else {
                    ensureBtAndListen(forceRescan = forceRescanPending)
                }
            } else {
                binding.statusText.text = getString(R.string.permission_needed)
            }
        }

    private val enableBtLauncher =
        registerForActivityResult(ActivityResultContracts.StartActivityForResult()) {
            if (isBluetoothEnabled()) {
                if (associateAfterBt) {
                    associateAfterBt = false
                    beginAssociate()
                } else {
                    startLinkService(forceRescan = !resumeListenAfterBtEnable && forceRescanPending)
                }
            } else {
                binding.statusText.text = getString(R.string.bt_off)
            }
            resumeListenAfterBtEnable = false
            forceRescanPending = false
        }

    private val associateChooserLauncher =
        registerForActivityResult(ActivityResultContracts.StartIntentSenderForResult()) { result ->
            if (result.resultCode != Activity.RESULT_OK) {
                binding.statusText.text = getString(R.string.associate_failed, "cancelled")
                return@registerForActivityResult
            }
            val addr = CompanionAssociation.addressFromChooserResult(result.data)
            if (addr != null) {
                onAssociated(addr)
            } else if (CompanionAssociation.isAssociated(this)) {
                val linked = CompanionAssociation.primaryAddress(this)
                if (linked != null) onAssociated(linked)
                else startLinkService(forceRescan = false)
            } else {
                // API 33+ may deliver address only via onAssociationCreated.
                startLinkService(forceRescan = false)
            }
        }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        binding = ActivityMainBinding.inflate(layoutInflater)
        setContentView(binding.root)

        store = PresetStore(this)
        sshStore = SshPresetStore(this)
        sshHostKeys = SshHostKeyStore(this)
        presets.clear()
        presets.addAll(store.load())
        sshPresets.clear()
        sshPresets.addAll(sshStore.load())

        listAdapter = ArrayAdapter(this, android.R.layout.simple_list_item_1, mutableListOf())
        binding.presetList.adapter = listAdapter
        sshListAdapter = ArrayAdapter(this, android.R.layout.simple_list_item_1, mutableListOf())
        binding.sshPresetList.adapter = sshListAdapter
        refreshPresetList()
        refreshSshPresetList()

        binding.linkButton.setOnClickListener { requestPermissionsAndAssociate() }
        binding.scanButton.setOnClickListener { requestPermissionsAndScan(force = true) }
        binding.disconnectButton.setOnClickListener {
            BleLinkService.stop(this)
            // Status comes from service (linked standby → Listening…).
        }
        binding.disconnectButton.setOnLongClickListener {
            confirmUnlink()
            true
        }
        binding.batterySaverSwitch.isChecked = CompanionPrefs.isBatterySaverEnabled(this)
        binding.batterySaverSwitch.setOnCheckedChangeListener { _, checked ->
            CompanionPrefs.setBatterySaverEnabled(this, checked)
            bleService?.client?.setBatterySaverEnabled(checked)
                ?: run {
                    // Service not bound yet — preference is stored; next listen picks it up.
                    Toast.makeText(
                        this,
                        if (checked) R.string.battery_saver_on_toast else R.string.battery_saver_off_toast,
                        Toast.LENGTH_SHORT
                    ).show()
                }
        }
        binding.addPresetButton.setOnClickListener {
            if (presets.size >= BleProtocol.HTTP_MAX_PRESETS) {
                Toast.makeText(
                    this,
                    getString(R.string.preset_limit, BleProtocol.HTTP_MAX_PRESETS),
                    Toast.LENGTH_SHORT
                ).show()
                return@setOnClickListener
            }
            showPresetDialog(null)
        }
        binding.presetList.setOnItemClickListener { _, _, position, _ ->
            showPresetDialog(position)
        }
        binding.presetList.setOnItemLongClickListener { _, _, position, _ ->
            confirmDelete(position)
            true
        }
        binding.addSshPresetButton.setOnClickListener {
            if (sshPresets.size >= BleProtocol.SSH_MAX_PRESETS) {
                Toast.makeText(
                    this,
                    getString(R.string.preset_limit, BleProtocol.SSH_MAX_PRESETS),
                    Toast.LENGTH_SHORT
                ).show()
                return@setOnClickListener
            }
            showSshPresetDialog(null)
        }
        binding.sshPresetList.setOnItemClickListener { _, _, position, _ ->
            showSshPresetDialog(position)
        }
        binding.sshPresetList.setOnItemLongClickListener { _, _, position, _ ->
            confirmDeleteSsh(position)
            true
        }
    }

    override fun onStart() {
        super.onStart()
        if (hasBlePermissions()) {
            if (CompanionAssociation.isAssociated(this)) {
                CompanionAssociation.startObserving(this)
                ensureBtAndListen(forceRescan = false)
            } else {
                binding.statusText.text = getString(R.string.status_idle)
            }
        } else {
            binding.statusText.text = getString(R.string.status_idle)
        }
    }

    override fun onStop() {
        if (bound) {
            bleService?.removeUiListener(this)
            unbindService(serviceConnection)
            bound = false
            bleService = null
        }
        // Do not stop the foreground service — keep listening while locked/background.
        super.onStop()
    }

    private fun isBluetoothEnabled(): Boolean {
        val mgr = getSystemService(Context.BLUETOOTH_SERVICE) as android.bluetooth.BluetoothManager
        return mgr.adapter?.isEnabled == true
    }

    private fun hasBlePermissions(): Boolean {
        val needed = mutableListOf(
            Manifest.permission.BLUETOOTH_SCAN,
            Manifest.permission.BLUETOOTH_CONNECT
        )
        if (Build.VERSION.SDK_INT >= 33) {
            needed.add(Manifest.permission.POST_NOTIFICATIONS)
        }
        return needed.all {
            ContextCompat.checkSelfPermission(this, it) == PackageManager.PERMISSION_GRANTED
        }
    }

    private fun refreshPresetList() {
        listAdapter.clear()
        for (p in presets) {
            listAdapter.add("${p.method.uppercase()}  ${p.name}\n${p.url}")
        }
        listAdapter.notifyDataSetChanged()
    }

    private fun refreshSshPresetList() {
        sshListAdapter.clear()
        for (p in sshPresets) {
            sshListAdapter.add("${p.name}\n${p.user}@${p.host}:${p.port} · ${p.command}")
        }
        sshListAdapter.notifyDataSetChanged()
    }

    private fun persistPresets() {
        store.save(presets)
        refreshPresetList()
    }

    private fun persistSshPresets() {
        sshStore.save(sshPresets)
        refreshSshPresetList()
    }

    private fun showPresetDialog(editIndex: Int?) {
        val dialogBinding = DialogPresetBinding.inflate(LayoutInflater.from(this))
        val existing = editIndex?.let { presets.getOrNull(it) }
        if (existing != null) {
            dialogBinding.editName.setText(existing.name)
            dialogBinding.editUrl.setText(existing.url)
            dialogBinding.editMethod.setText(existing.method)
            dialogBinding.editHeaders.setText(existing.headers)
            dialogBinding.editBody.setText(existing.body)
        }
        val builder = AlertDialog.Builder(this)
            .setTitle(if (editIndex == null) R.string.add_preset else R.string.edit_preset)
            .setView(dialogBinding.root)
            .setPositiveButton(R.string.save, null)
            .setNegativeButton(R.string.cancel, null)
        if (editIndex != null) {
            builder.setNeutralButton(R.string.delete_preset) { _, _ ->
                presets.removeAt(editIndex)
                persistPresets()
            }
        }
        val dialog = builder.create()
        dialog.setOnShowListener {
            dialogBinding.testRunButton.setOnClickListener {
                val url = dialogBinding.editUrl.text?.toString()?.trim().orEmpty()
                if (url.isEmpty()) {
                    Toast.makeText(this, R.string.test_run_http_url_required, Toast.LENGTH_SHORT).show()
                    return@setOnClickListener
                }
                val preset = HttpPreset(
                    name = dialogBinding.editName.text?.toString()?.trim().orEmpty().ifBlank { "test" },
                    url = url,
                    method = dialogBinding.editMethod.text?.toString()?.trim()?.uppercase().orEmpty()
                        .ifBlank { "GET" },
                    headers = dialogBinding.editHeaders.text?.toString().orEmpty(),
                    body = dialogBinding.editBody.text?.toString().orEmpty()
                )
                runHttpTest(preset, dialogBinding.testRunButton)
            }
            dialog.getButton(AlertDialog.BUTTON_POSITIVE).setOnClickListener {
                val name = dialogBinding.editName.text?.toString()?.trim().orEmpty()
                val url = dialogBinding.editUrl.text?.toString()?.trim().orEmpty()
                val method = dialogBinding.editMethod.text?.toString()?.trim()?.uppercase().orEmpty()
                    .ifBlank { "GET" }
                val headers = dialogBinding.editHeaders.text?.toString().orEmpty()
                val body = dialogBinding.editBody.text?.toString().orEmpty()
                if (name.isEmpty() || url.isEmpty()) {
                    Toast.makeText(this, "Name and URL required", Toast.LENGTH_SHORT).show()
                    return@setOnClickListener
                }
                val preset = HttpPreset(name, url, method, headers, body)
                if (editIndex == null) {
                    presets.add(preset)
                } else {
                    presets[editIndex] = preset
                }
                persistPresets()
                dialog.dismiss()
            }
        }
        dialog.show()
    }

    private fun showSshPresetDialog(editIndex: Int?) {
        val dialogBinding = DialogSshPresetBinding.inflate(LayoutInflater.from(this))
        val existing = editIndex?.let { sshPresets.getOrNull(it) }
        if (existing != null) {
            dialogBinding.editName.setText(existing.name)
            dialogBinding.editHost.setText(existing.host)
            dialogBinding.editPort.setText(existing.port.toString())
            dialogBinding.editUser.setText(existing.user)
            dialogBinding.editCommand.setText(existing.command)
            dialogBinding.editPassword.setText(existing.password)
        }
        val builder = AlertDialog.Builder(this)
            .setTitle(if (editIndex == null) R.string.add_ssh_preset else R.string.edit_ssh_preset)
            .setView(dialogBinding.root)
            .setPositiveButton(R.string.save, null)
            .setNegativeButton(R.string.cancel, null)
        if (editIndex != null) {
            builder.setNeutralButton(R.string.delete_preset) { _, _ ->
                sshPresets.removeAt(editIndex)
                persistSshPresets()
            }
        }
        val dialog = builder.create()
        dialog.setOnShowListener {
            dialogBinding.testRunButton.setOnClickListener {
                val host = dialogBinding.editHost.text?.toString()?.trim().orEmpty()
                val user = dialogBinding.editUser.text?.toString()?.trim().orEmpty()
                val command = dialogBinding.editCommand.text?.toString()?.trim().orEmpty()
                if (host.isEmpty() || user.isEmpty() || command.isEmpty()) {
                    Toast.makeText(this, R.string.test_run_ssh_required, Toast.LENGTH_SHORT).show()
                    return@setOnClickListener
                }
                val port = dialogBinding.editPort.text?.toString()?.trim()?.toIntOrNull() ?: 22
                val preset = SshPreset(
                    id = existing?.id ?: "test-run",
                    name = dialogBinding.editName.text?.toString()?.trim().orEmpty().ifBlank { "test" },
                    host = host,
                    port = port.coerceIn(1, 65535),
                    user = user,
                    command = command,
                    password = dialogBinding.editPassword.text?.toString().orEmpty()
                )
                runSshTest(preset, dialogBinding.testRunButton)
            }
            dialog.getButton(AlertDialog.BUTTON_POSITIVE).setOnClickListener {
                val name = dialogBinding.editName.text?.toString()?.trim().orEmpty()
                val host = dialogBinding.editHost.text?.toString()?.trim().orEmpty()
                val port = dialogBinding.editPort.text?.toString()?.trim()?.toIntOrNull() ?: 22
                val user = dialogBinding.editUser.text?.toString()?.trim().orEmpty()
                val command = dialogBinding.editCommand.text?.toString()?.trim().orEmpty()
                val password = dialogBinding.editPassword.text?.toString().orEmpty()
                if (name.isEmpty() || host.isEmpty() || user.isEmpty() || command.isEmpty()) {
                    Toast.makeText(this, R.string.ssh_fields_required, Toast.LENGTH_SHORT).show()
                    return@setOnClickListener
                }
                val preset = SshPreset(
                    id = existing?.id ?: UUID.randomUUID().toString(),
                    name = name,
                    host = host,
                    port = port.coerceIn(1, 65535),
                    user = user,
                    command = command,
                    password = password
                )
                if (editIndex == null) {
                    sshPresets.add(preset)
                } else {
                    sshPresets[editIndex] = preset
                }
                persistSshPresets()
                dialog.dismiss()
            }
        }
        dialog.show()
    }

    private fun runHttpTest(preset: HttpPreset, button: android.widget.Button) {
        if (testRunInFlight) return
        testRunInFlight = true
        val label = button.text
        button.isEnabled = false
        button.setText(R.string.test_run_running)
        HttpExecutor.executeAsync(preset) { result ->
            runOnUiThread {
                testRunInFlight = false
                button.isEnabled = true
                button.text = label
                val bodyText = decodeBody(result.body)
                val title = if (result.errCode != 0) {
                    getString(R.string.test_run_http_err_title)
                } else {
                    getString(R.string.test_run_http_title, result.httpStatus)
                }
                val summary = if (result.errCode != 0) httpErrLabel(result.errCode) else null
                showTestResult(title, summary, bodyText, result.log)
            }
        }
    }

    private fun runSshTest(preset: SshPreset, button: android.widget.Button) {
        if (testRunInFlight) return
        testRunInFlight = true
        val label = button.text
        button.isEnabled = false
        button.setText(R.string.test_run_running)
        SshExecutor.executeAsync(preset, sshHostKeys) { result ->
            runOnUiThread {
                testRunInFlight = false
                button.isEnabled = true
                button.text = label
                val bodyText = decodeBody(result.body)
                val title = if (result.errCode != 0) {
                    getString(R.string.test_run_ssh_err_title)
                } else {
                    getString(R.string.test_run_ssh_title, result.exitStatus)
                }
                val summary = if (result.errCode != 0) sshErrLabel(result.errCode) else null
                showTestResult(title, summary, bodyText, result.log)
            }
        }
    }

    private fun decodeBody(body: ByteArray): String {
        if (body.isEmpty()) return getString(R.string.test_run_empty_body)
        return String(body, StandardCharsets.UTF_8)
    }

    private fun httpErrLabel(code: Int): String = when (code) {
        BleProtocol.ERR_HTTP_TIMEOUT -> getString(R.string.http_err_timeout)
        BleProtocol.ERR_HTTP_FAILED -> getString(R.string.http_err_failed)
        else -> "err $code"
    }

    private fun sshErrLabel(code: Int): String = when (code) {
        BleProtocol.ERR_SSH_TIMEOUT -> getString(R.string.ssh_err_timeout)
        BleProtocol.ERR_SSH_AUTH_FAILED -> getString(R.string.ssh_err_auth)
        BleProtocol.ERR_SSH_HOST_KEY -> getString(R.string.ssh_err_host_key)
        BleProtocol.ERR_SSH_FAILED -> getString(R.string.ssh_err_failed)
        else -> "err $code"
    }

    private fun showTestResult(title: String, summary: String?, body: String, log: String) {
        val pad = (16 * resources.displayMetrics.density).toInt()
        val text = buildString {
            if (!summary.isNullOrBlank()) {
                append(summary)
                append("\n\n")
            }
            append(getString(R.string.test_run_section_body))
            append('\n')
            append(body)
            append("\n\n")
            append(getString(R.string.test_run_section_log))
            append('\n')
            append(log.ifBlank { "(no log)" })
        }
        val textView = TextView(this).apply {
            this.text = text
            typeface = android.graphics.Typeface.MONOSPACE
            textSize = 13f
            setPadding(pad, pad, pad, pad)
            setTextIsSelectable(true)
        }
        val scroll = ScrollView(this).apply {
            addView(textView)
            layoutParams = android.widget.FrameLayout.LayoutParams(
                android.widget.FrameLayout.LayoutParams.MATCH_PARENT,
                (resources.displayMetrics.heightPixels * 0.55f).toInt()
            )
        }
        AlertDialog.Builder(this)
            .setTitle(title)
            .setView(scroll)
            .setPositiveButton(R.string.ok, null)
            .setNeutralButton(R.string.test_run_copy_log) { _, _ ->
                val clip = android.content.ClipData.newPlainText("test-run-log", log.ifBlank { text })
                (getSystemService(Context.CLIPBOARD_SERVICE) as android.content.ClipboardManager)
                    .setPrimaryClip(clip)
                Toast.makeText(this, R.string.test_run_log_copied, Toast.LENGTH_SHORT).show()
            }
            .show()
    }

    private fun confirmDelete(index: Int) {
        val name = presets.getOrNull(index)?.name ?: return
        AlertDialog.Builder(this)
            .setMessage("Delete \"$name\"?")
            .setPositiveButton(R.string.delete_preset) { _, _ ->
                presets.removeAt(index)
                persistPresets()
            }
            .setNegativeButton(R.string.cancel, null)
            .show()
    }

    private fun confirmDeleteSsh(index: Int) {
        val name = sshPresets.getOrNull(index)?.name ?: return
        AlertDialog.Builder(this)
            .setMessage("Delete \"$name\"?")
            .setPositiveButton(R.string.delete_preset) { _, _ ->
                sshPresets.removeAt(index)
                persistSshPresets()
            }
            .setNegativeButton(R.string.cancel, null)
            .show()
    }

    private fun confirmUnlink() {
        AlertDialog.Builder(this)
            .setTitle(R.string.unlink)
            .setMessage(R.string.unlink_confirm)
            .setPositiveButton(R.string.unlink) { _, _ ->
                BleLinkService.unlink(this)
                binding.statusText.text = getString(R.string.status_idle)
            }
            .setNegativeButton(R.string.cancel, null)
            .show()
    }

    private fun requestPermissionsAndScan(force: Boolean) {
        forceRescanPending = force
        associateAfterBt = false
        val missing = missingBlePermissions()
        if (missing.isEmpty()) {
            ensureBtAndListen(forceRescan = force)
        } else {
            permissionLauncher.launch(missing.toTypedArray())
        }
    }

    private fun requestPermissionsAndAssociate() {
        associateAfterBt = true
        forceRescanPending = false
        val missing = missingBlePermissions()
        if (missing.isEmpty()) {
            ensureBtAndAssociate()
        } else {
            permissionLauncher.launch(missing.toTypedArray())
        }
    }

    private fun missingBlePermissions(): List<String> {
        val needed = mutableListOf(
            Manifest.permission.BLUETOOTH_SCAN,
            Manifest.permission.BLUETOOTH_CONNECT
        )
        if (Build.VERSION.SDK_INT >= 33) {
            needed.add(Manifest.permission.POST_NOTIFICATIONS)
        }
        return needed.filter {
            ContextCompat.checkSelfPermission(this, it) != PackageManager.PERMISSION_GRANTED
        }
    }

    private fun ensureBtAndListen(forceRescan: Boolean) {
        if (!isBluetoothEnabled()) {
            binding.statusText.text = getString(R.string.bt_off)
            resumeListenAfterBtEnable = !forceRescan
            forceRescanPending = forceRescan
            enableBtLauncher.launch(Intent(BluetoothAdapter.ACTION_REQUEST_ENABLE))
            return
        }
        startLinkService(forceRescan)
    }

    private fun ensureBtAndAssociate() {
        if (!isBluetoothEnabled()) {
            binding.statusText.text = getString(R.string.bt_off)
            associateAfterBt = true
            enableBtLauncher.launch(Intent(BluetoothAdapter.ACTION_REQUEST_ENABLE))
            return
        }
        beginAssociate()
    }

    private fun beginAssociate() {
        val lm = getSystemService(Context.LOCATION_SERVICE) as? android.location.LocationManager
        val locationOn = lm != null && (
            lm.isProviderEnabled(android.location.LocationManager.GPS_PROVIDER) ||
                lm.isProviderEnabled(android.location.LocationManager.NETWORK_PROVIDER)
            )
        if (!locationOn) {
            binding.statusText.text = getString(R.string.associate_need_location)
            Toast.makeText(this, R.string.associate_need_location, Toast.LENGTH_LONG).show()
            try {
                startActivity(Intent(Settings.ACTION_LOCATION_SOURCE_SETTINGS))
            } catch (_: Exception) {
            }
            return
        }

        // Connected GATT ⇒ device stopped advertising ⇒ CDM list stays empty forever.
        if (bleService?.client?.isConnected() == true) {
            AlertDialog.Builder(this)
                .setTitle(R.string.associate_while_connected_title)
                .setMessage(R.string.associate_while_connected)
                .setPositiveButton(R.string.associate_stop_now) { _, _ ->
                    BleLinkService.stop(this)
                    binding.statusText.text = getString(R.string.disconnected)
                    Toast.makeText(this, R.string.associate_hint, Toast.LENGTH_LONG).show()
                }
                .setNegativeButton(R.string.cancel, null)
                .show()
            return
        }

        Toast.makeText(this, R.string.associate_hint, Toast.LENGTH_LONG).show()
        val preferred = bleService?.client?.lastPeerAddress()
            ?: CompanionAssociation.primaryAddress(this)
            ?: CompanionAssociation.rememberedAddress(this)

        CompanionAssociation.associate(
            context = this,
            executor = mainExecutor,
            onPending = { sender: IntentSender ->
                associateChooserLauncher.launch(IntentSenderRequest.Builder(sender).build())
            },
            onCreated = { addr -> onAssociated(addr) },
            onFailure = { err ->
                binding.statusText.text =
                    getString(R.string.associate_failed, err?.toString() ?: "unknown")
                Toast.makeText(this, R.string.associate_try_scan_first, Toast.LENGTH_LONG).show()
            },
            preferredAddress = preferred
        )
    }

    private fun onAssociated(address: String) {
        CompanionAssociation.saveAddress(this, address)
        CompanionAssociation.startObserving(this)
        Toast.makeText(this, getString(R.string.associate_ok, address), Toast.LENGTH_SHORT).show()
        startLinkService(forceRescan = false)
        maybePromptBatteryExemption()
    }

    private fun startLinkService(forceRescan: Boolean) {
        forceRescanPending = forceRescan
        if (forceRescan) {
            BleLinkService.startScan(this)
        } else {
            BleLinkService.startListening(this)
        }
        if (!bound) {
            bindService(
                Intent(this, BleLinkService::class.java),
                serviceConnection,
                Context.BIND_AUTO_CREATE
            )
        } else if (forceRescan) {
            bleService?.client?.startScan()
            forceRescanPending = false
        } else {
            bleService?.client?.startListening(forceRescan = false)
            forceRescanPending = false
        }
        maybePromptBatteryExemption()
    }

    /**
     * OEM Doze / battery savers freeze BLE even with companion presence on some phones.
     * Ask once for unrestricted battery use.
     */
    private fun maybePromptBatteryExemption() {
        val pm = getSystemService(PowerManager::class.java) ?: return
        if (pm.isIgnoringBatteryOptimizations(packageName)) return
        val prefs = getSharedPreferences("companion_ui", MODE_PRIVATE)
        if (prefs.getBoolean(KEY_BATTERY_PROMPT_SHOWN, false)) return
        prefs.edit().putBoolean(KEY_BATTERY_PROMPT_SHOWN, true).apply()
        AlertDialog.Builder(this)
            .setTitle(R.string.battery_title)
            .setMessage(R.string.battery_message)
            .setPositiveButton(R.string.battery_allow) { _, _ ->
                try {
                    startActivity(
                        Intent(Settings.ACTION_REQUEST_IGNORE_BATTERY_OPTIMIZATIONS).apply {
                            data = Uri.parse("package:$packageName")
                        }
                    )
                } catch (_: Exception) {
                    startActivity(Intent(Settings.ACTION_IGNORE_BATTERY_OPTIMIZATION_SETTINGS))
                }
            }
            .setNegativeButton(R.string.cancel, null)
            .show()
    }

    override fun presets(): List<HttpPreset> = presets.toList()

    override fun sshPresets(): List<SshPreset> = sshPresets.toList()

    override fun onStatus(message: String) {
        binding.statusText.text = message
    }

    override fun onDetail(message: String) {
        binding.detailText.text = message
    }

    override fun onHello(firmwareVersion: String) {
        Toast.makeText(this, getString(R.string.hello_ok, firmwareVersion), Toast.LENGTH_SHORT)
            .show()
    }

    override fun onDisconnected() {
        // Presence / autoConnect resumes when Cardputer advertises again.
    }

    companion object {
        private const val KEY_BATTERY_PROMPT_SHOWN = "battery_opt_prompt_shown"
    }
}
