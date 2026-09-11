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
import android.widget.Toast
import androidx.activity.result.IntentSenderRequest
import androidx.activity.result.contract.ActivityResultContracts
import androidx.appcompat.app.AlertDialog
import androidx.appcompat.app.AppCompatActivity
import androidx.core.content.ContextCompat
import com.cardputer.companion.databinding.ActivityMainBinding
import com.cardputer.companion.databinding.DialogPresetBinding
import java.util.concurrent.Executor

class MainActivity : AppCompatActivity(), BleLinkClient.Listener {
    private lateinit var binding: ActivityMainBinding
    private lateinit var store: PresetStore
    private val presets = mutableListOf<HttpPreset>()
    private lateinit var listAdapter: ArrayAdapter<String>
    private var resumeListenAfterBtEnable = false
    private var forceRescanPending = false
    private var associateAfterBt = false

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
        presets.clear()
        presets.addAll(store.load())

        listAdapter = ArrayAdapter(this, android.R.layout.simple_list_item_1, mutableListOf())
        binding.presetList.adapter = listAdapter
        refreshPresetList()

        binding.linkButton.setOnClickListener { requestPermissionsAndAssociate() }
        binding.scanButton.setOnClickListener { requestPermissionsAndScan(force = true) }
        binding.disconnectButton.setOnClickListener {
            BleLinkService.stop(this)
            binding.statusText.text = getString(R.string.disconnected)
        }
        binding.disconnectButton.setOnLongClickListener {
            confirmUnlink()
            true
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

    private fun persistPresets() {
        store.save(presets)
        refreshPresetList()
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
