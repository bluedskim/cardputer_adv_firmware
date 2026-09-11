package com.cardputer.companion

import android.companion.CompanionDeviceService
import android.util.Log

/**
 * Bound by the system when an associated CardputerAdv is nearby / advertising
 * (watch-style presence). Starts the BLE foreground service and connects.
 */
class CardputerCompanionService : CompanionDeviceService() {
    @Deprecated("Deprecated in Java")
    override fun onDeviceAppeared(address: String) {
        Log.i(TAG, "device appeared: $address")
        CompanionAssociation.saveAddress(this, address)
        BleLinkService.onAssociatedDeviceAppeared(this, address)
    }

    @Deprecated("Deprecated in Java")
    override fun onDeviceDisappeared(address: String) {
        Log.i(TAG, "device disappeared: $address")
        // Keep association; Cardputer only advertises while Connect/Web is open.
    }

    companion object {
        private const val TAG = "CardputerPresence"
    }
}
