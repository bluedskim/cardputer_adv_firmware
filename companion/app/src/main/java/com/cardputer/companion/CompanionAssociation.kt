package com.cardputer.companion

import android.annotation.SuppressLint
import android.bluetooth.BluetoothDevice
import android.bluetooth.le.ScanFilter
import android.bluetooth.le.ScanResult
import android.companion.AssociationInfo
import android.companion.AssociationRequest
import android.companion.BluetoothLeDeviceFilter
import android.companion.CompanionDeviceManager
import android.content.Context
import android.content.IntentSender
import android.os.Build
import android.os.ParcelUuid
import java.util.UUID
import java.util.concurrent.Executor
import java.util.regex.Pattern

/**
 * Watch-style companion association: system presence observation wakes the app
 * when CardputerAdv advertises, even with the screen locked / UI backgrounded.
 */
object CompanionAssociation {
    private const val PREFS = "companion_assoc"
    private const val KEY_ADDR = "associated_address"

    fun manager(context: Context): CompanionDeviceManager =
        context.getSystemService(Context.COMPANION_DEVICE_SERVICE) as CompanionDeviceManager

    fun rememberedAddress(context: Context): String? =
        context.getSharedPreferences(PREFS, Context.MODE_PRIVATE)
            .getString(KEY_ADDR, null)
            ?.let { normalizeBluetoothAddress(it) }

    fun saveAddress(context: Context, address: String) {
        val normalized = normalizeBluetoothAddress(address) ?: return
        context.getSharedPreferences(PREFS, Context.MODE_PRIVATE)
            .edit()
            .putString(KEY_ADDR, normalized)
            .apply()
    }

    fun clearAddress(context: Context) {
        context.getSharedPreferences(PREFS, Context.MODE_PRIVATE)
            .edit()
            .remove(KEY_ADDR)
            .apply()
    }

    fun isAssociated(context: Context): Boolean =
        associatedAddresses(context).isNotEmpty()

    @SuppressLint("MissingPermission")
    fun associatedAddresses(context: Context): List<String> {
        val cdm = manager(context)
        return if (Build.VERSION.SDK_INT >= 33) {
            cdm.myAssociations.mapNotNull { info ->
                // MacAddress.toString() is lowercase; getRemoteDevice requires UPPERCASE.
                info.deviceMacAddress?.toString()?.let { normalizeBluetoothAddress(it) }
            }
        } else {
            @Suppress("DEPRECATION")
            cdm.associations.mapNotNull { normalizeBluetoothAddress(it) }
        }
    }

    fun primaryAddress(context: Context): String? =
        associatedAddresses(context).firstOrNull() ?: rememberedAddress(context)

    /**
     * BluetoothAdapter.getRemoteDevice / checkBluetoothAddress require
     * uppercase "AA:BB:CC:DD:EE:FF". CDM MacAddress.toString() is lowercase.
     */
    fun normalizeBluetoothAddress(raw: String?): String? {
        if (raw.isNullOrBlank()) return null
        var s = raw.trim()
        // MacAddress / some OEMs use '-' or no separators.
        s = s.replace('-', ':')
        if (!s.contains(':') && s.length == 12) {
            s = s.chunked(2).joinToString(":")
        }
        s = s.uppercase(java.util.Locale.US)
        return if (android.bluetooth.BluetoothAdapter.checkBluetoothAddress(s)) s else null
    }

    fun buildAssociationRequest(preferredAddress: String? = null): AssociationRequest {
        // OR filters: CDM / OEMs often fail on 128-bit UUID ScanFilter. Name in
        // primary ADV is the reliable match. Also try last known MAC if any.
        val builder = AssociationRequest.Builder().setSingleDevice(false)

        builder.addDeviceFilter(
            BluetoothLeDeviceFilter.Builder()
                .setNamePattern(Pattern.compile("^CardputerAdv$"))
                .build()
        )
        builder.addDeviceFilter(
            BluetoothLeDeviceFilter.Builder()
                .setNamePattern(Pattern.compile("CardputerAdv"))
                .build()
        )

        // Optional UUID filter (works on some Pixels; ignored if ADV has no UUID).
        val serviceUuid = ParcelUuid(UUID.fromString(BleProtocol.SERVICE_UUID))
        builder.addDeviceFilter(
            BluetoothLeDeviceFilter.Builder()
                .setScanFilter(
                    ScanFilter.Builder().setServiceUuid(serviceUuid).build()
                )
                .build()
        )

        if (!preferredAddress.isNullOrBlank()) {
            try {
                builder.addDeviceFilter(
                    BluetoothLeDeviceFilter.Builder()
                        .setScanFilter(
                            ScanFilter.Builder().setDeviceAddress(preferredAddress).build()
                        )
                        .build()
                )
            } catch (_: IllegalArgumentException) {
            }
        }

        return builder.build()
    }

    /**
     * Starts the system association UI. [onPending] must launch the IntentSender
     * (user picks CardputerAdv). On API 33+ [onCreated] also fires.
     */
    fun associate(
        context: Context,
        executor: Executor,
        onPending: (IntentSender) -> Unit,
        onCreated: (String) -> Unit,
        onFailure: (CharSequence?) -> Unit,
        preferredAddress: String? = null
    ) {
        val cdm = manager(context)
        val request = buildAssociationRequest(preferredAddress)
        if (Build.VERSION.SDK_INT >= 33) {
            cdm.associate(
                request,
                executor,
                object : CompanionDeviceManager.Callback() {
                    override fun onAssociationPending(intentSender: IntentSender) {
                        onPending(intentSender)
                    }

                    override fun onAssociationCreated(associationInfo: AssociationInfo) {
                        val addr = associationInfo.deviceMacAddress?.toString()
                        if (addr != null) {
                            saveAddress(context, addr)
                            onCreated(addr)
                        } else {
                            onFailure("No MAC on association")
                        }
                    }

                    override fun onFailure(errorMessage: CharSequence?) {
                        onFailure(errorMessage)
                    }
                }
            )
        } else {
            @Suppress("DEPRECATION")
            cdm.associate(
                request,
                object : CompanionDeviceManager.Callback() {
                    @Deprecated("Deprecated in Java")
                    override fun onDeviceFound(intentSender: IntentSender) {
                        onPending(intentSender)
                    }

                    override fun onFailure(errorMessage: CharSequence?) {
                        onFailure(errorMessage)
                    }
                },
                null
            )
        }
    }

    fun addressFromChooserResult(data: android.content.Intent?): String? {
        if (data == null) return null
        if (Build.VERSION.SDK_INT >= 33) {
            data.getParcelableExtra(
                CompanionDeviceManager.EXTRA_DEVICE,
                BluetoothDevice::class.java
            )?.address?.let { return it }
            data.getParcelableExtra(
                CompanionDeviceManager.EXTRA_DEVICE,
                ScanResult::class.java
            )?.device?.address?.let { return it }
            return null
        }
        @Suppress("DEPRECATION")
        val extra = data.getParcelableExtra<android.os.Parcelable>(CompanionDeviceManager.EXTRA_DEVICE)
        return when (extra) {
            is BluetoothDevice -> extra.address
            is ScanResult -> extra.device.address
            else -> null
        }
    }

    /** System low-power presence scan — same mechanism watch apps use. */
    fun startObserving(context: Context) {
        val cdm = manager(context)
        val addrs = associatedAddresses(context)
        if (addrs.isEmpty()) return
        for (addr in addrs) {
            try {
                @Suppress("DEPRECATION")
                cdm.startObservingDevicePresence(addr)
            } catch (_: Exception) {
            }
        }
    }

    fun stopObserving(context: Context) {
        val cdm = manager(context)
        val addrs = associatedAddresses(context)
        if (addrs.isEmpty()) return
        for (addr in addrs) {
            try {
                @Suppress("DEPRECATION")
                cdm.stopObservingDevicePresence(addr)
            } catch (_: Exception) {
            }
        }
    }

    fun disassociateAll(context: Context) {
        stopObserving(context)
        val cdm = manager(context)
        if (Build.VERSION.SDK_INT >= 33) {
            cdm.myAssociations.forEach { info ->
                try {
                    cdm.disassociate(info.id)
                } catch (_: Exception) {
                }
            }
        } else {
            @Suppress("DEPRECATION")
            cdm.associations.toList().forEach { addr ->
                try {
                    @Suppress("DEPRECATION")
                    cdm.disassociate(addr)
                } catch (_: Exception) {
                }
            }
        }
        clearAddress(context)
    }
}
