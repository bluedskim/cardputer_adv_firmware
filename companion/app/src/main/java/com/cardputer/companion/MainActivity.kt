package com.cardputer.companion

import android.Manifest
import android.bluetooth.BluetoothAdapter
import android.content.Intent
import android.content.pm.PackageManager
import android.os.Bundle
import android.widget.Toast
import androidx.activity.result.contract.ActivityResultContracts
import androidx.appcompat.app.AppCompatActivity
import androidx.core.content.ContextCompat
import com.cardputer.companion.databinding.ActivityMainBinding

class MainActivity : AppCompatActivity(), BleLinkClient.Listener {
    private lateinit var binding: ActivityMainBinding
    private lateinit var client: BleLinkClient

    private val permissionLauncher =
        registerForActivityResult(ActivityResultContracts.RequestMultiplePermissions()) { result ->
            val granted = result.values.all { it }
            if (granted) {
                ensureBtAndScan()
            } else {
                binding.statusText.text = getString(R.string.permission_needed)
            }
        }

    private val enableBtLauncher =
        registerForActivityResult(ActivityResultContracts.StartActivityForResult()) {
            if (client.isBluetoothEnabled()) {
                client.startScan()
            } else {
                binding.statusText.text = getString(R.string.bt_off)
            }
        }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        binding = ActivityMainBinding.inflate(layoutInflater)
        setContentView(binding.root)

        client = BleLinkClient(this, this)

        binding.scanButton.setOnClickListener { requestPermissionsAndScan() }
        binding.disconnectButton.setOnClickListener {
            client.disconnect()
            binding.statusText.text = getString(R.string.disconnected)
        }
    }

    override fun onDestroy() {
        client.disconnect()
        super.onDestroy()
    }

    private fun requestPermissionsAndScan() {
        val needed = arrayOf(
            Manifest.permission.BLUETOOTH_SCAN,
            Manifest.permission.BLUETOOTH_CONNECT
        )
        val missing = needed.filter {
            ContextCompat.checkSelfPermission(this, it) != PackageManager.PERMISSION_GRANTED
        }
        if (missing.isEmpty()) {
            ensureBtAndScan()
        } else {
            permissionLauncher.launch(missing.toTypedArray())
        }
    }

    private fun ensureBtAndScan() {
        if (!client.isBluetoothEnabled()) {
            binding.statusText.text = getString(R.string.bt_off)
            enableBtLauncher.launch(Intent(BluetoothAdapter.ACTION_REQUEST_ENABLE))
            return
        }
        client.startScan()
    }

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
        // status already updated by client
    }
}
