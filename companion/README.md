# Cardputer Companion (Android, M2)

BLE central for [cardputer_adv_firmware](../README.md): M1 link + **HTTP Web commander** presets.

Designed like a **smartwatch companion** (Mi Band–style):

1. **Link** once via Android Companion Device Manager
2. System **device presence** wakes the app when CardputerAdv advertises (lock screen / background)
3. Foreground service + GATT Ping keep the session alive while connected

Also: HTTP presets on phone; serves `HttpList*` / `HttpRun*` for the Cardputer **Web** app. No notification mirror or SSH yet (M3/M4).

Protocol: [docs/ble_link.md](../docs/ble_link.md). UUIDs must match `include/ble/ble_protocol.h` and `BleProtocol.kt`.

## Requirements

- Android Studio Ladybug+ (AGP 8.7)
- Physical BLE device (emulator is weak for companion presence)
- minSdk **31** (Android 12)
- Location services **on** (required by Companion Device Manager association UI)
- Unrestricted battery recommended on OEM phones

## Build

```bash
cd companion
./gradlew :app:assembleDebug
```

`local.properties` example: `sdk.dir=/home/YOU/Android/Sdk`

## Watch-style setup

1. On Cardputer: **Settings → BLE → Pair** (first time), then leave advertising (Pair/Connect or open **Web**).
2. Open Companion → grant Bluetooth + notifications → tap **Link**.
3. System chooser: select **CardputerAdv** → association created → status **Linked — waiting…**
4. Allow **unrestricted battery** when prompted.
5. Lock the phone. Open **Web** on Cardputer — phone should wake via presence and connect.
6. **Stop** ends the foreground session but **keeps the link** (presence can wake again). Long-press **Stop** → **Unlink** to remove association.
7. **Scan** is a manual force-rescan fallback (no new association).

## Web commander

1. Edit presets in the Companion list (defaults included).
2. Linked Companion (UI, lock screen, or presence-woken) → open **Web** on Cardputer.
3. Phone performs HTTPS; body ≤512 B chunked over BLE.
4. Leaving **Web** disconnects on device; phone waits again via presence/autoConnect.

See [docs/m2_verify.md](../docs/m2_verify.md).

## Permissions / OEM notes

- Companion permissions: run/use-data/start-FGS-from-background + observe presence
- Battery: Settings → Apps → Companion → Battery → Unrestricted
- Samsung/Xiaomi: also check Sleeping apps / Autostart if presence never fires
