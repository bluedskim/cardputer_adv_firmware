package com.cardputer.companion

import android.Manifest
import android.bluetooth.BluetoothAdapter
import android.content.Intent
import android.content.pm.PackageManager
import android.os.Bundle
import android.view.LayoutInflater
import android.widget.ArrayAdapter
import android.widget.Toast
import androidx.activity.result.contract.ActivityResultContracts
import androidx.appcompat.app.AlertDialog
import androidx.appcompat.app.AppCompatActivity
import androidx.core.content.ContextCompat
import com.cardputer.companion.databinding.ActivityMainBinding
import com.cardputer.companion.databinding.DialogPresetBinding

class MainActivity : AppCompatActivity(), BleLinkClient.Listener {
    private lateinit var binding: ActivityMainBinding
    private lateinit var client: BleLinkClient
    private lateinit var store: PresetStore
    private val presets = mutableListOf<HttpPreset>()
    private lateinit var listAdapter: ArrayAdapter<String>
    private var resumeListenAfterBtEnable = false

    private val permissionLauncher =
        registerForActivityResult(ActivityResultContracts.RequestMultiplePermissions()) { result ->
            val granted = result.values.all { it }
            if (granted) {
                ensureBtAndListen(forceRescan = false)
            } else {
                binding.statusText.text = getString(R.string.permission_needed)
            }
        }

    private val enableBtLauncher =
        registerForActivityResult(ActivityResultContracts.StartActivityForResult()) {
            if (client.isBluetoothEnabled()) {
                if (resumeListenAfterBtEnable) {
                    client.startListening(forceRescan = false)
                } else {
                    client.startScan()
                }
            } else {
                binding.statusText.text = getString(R.string.bt_off)
            }
            resumeListenAfterBtEnable = false
        }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        binding = ActivityMainBinding.inflate(layoutInflater)
        setContentView(binding.root)

        store = PresetStore(this)
        presets.clear()
        presets.addAll(store.load())

        client = BleLinkClient(this, this)

        listAdapter = ArrayAdapter(this, android.R.layout.simple_list_item_1, mutableListOf())
        binding.presetList.adapter = listAdapter
        refreshPresetList()

        binding.scanButton.setOnClickListener { requestPermissionsAndScan(force = true) }
        binding.disconnectButton.setOnClickListener {
            // Explicit disconnect: do not bounce back into auto-listen until Scan / reopen.
            client.disconnect(resumeListen = false)
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
            ensureBtAndListen(forceRescan = false)
        } else {
            binding.statusText.text = getString(R.string.status_idle)
        }
    }

    override fun onStop() {
        // Keep GATT if connected; only stop scanning while backgrounded.
        client.stopListening()
        super.onStop()
    }

    override fun onDestroy() {
        client.disconnect(resumeListen = false)
        super.onDestroy()
    }

    private fun hasBlePermissions(): Boolean {
        val needed = arrayOf(
            Manifest.permission.BLUETOOTH_SCAN,
            Manifest.permission.BLUETOOTH_CONNECT
        )
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

    private fun requestPermissionsAndScan(force: Boolean) {
        val needed = arrayOf(
            Manifest.permission.BLUETOOTH_SCAN,
            Manifest.permission.BLUETOOTH_CONNECT
        )
        val missing = needed.filter {
            ContextCompat.checkSelfPermission(this, it) != PackageManager.PERMISSION_GRANTED
        }
        if (missing.isEmpty()) {
            ensureBtAndListen(forceRescan = force)
        } else {
            permissionLauncher.launch(missing.toTypedArray())
        }
    }

    private fun ensureBtAndListen(forceRescan: Boolean) {
        if (!client.isBluetoothEnabled()) {
            binding.statusText.text = getString(R.string.bt_off)
            resumeListenAfterBtEnable = !forceRescan
            enableBtLauncher.launch(Intent(BluetoothAdapter.ACTION_REQUEST_ENABLE))
            return
        }
        if (forceRescan) {
            client.startScan()
        } else {
            client.startListening(forceRescan = false)
        }
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
        // Auto-listen resumes inside BleLinkClient when listenWanted.
    }
}
