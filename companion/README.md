# Cardputer Companion (Android, M2)

BLE central for [cardputer_adv_firmware](../README.md): M1 link + **HTTP Web commander** presets.

- Foreground service keeps **Listening** while the screen is locked or the UI is backgrounded (ongoing notification)
- Remembers last peer address for faster reconnect; **Scan** forces a fresh search
- HTTP presets on phone (name, URL, method, headers, body)
- Serves `HttpList*` / `HttpRun*` when the Cardputer **Web** app requests them
- No notification mirror or SSH yet (M3/M4)

Protocol: [docs/ble_link.md](../docs/ble_link.md). UUIDs must match `include/ble/ble_protocol.h` and `BleProtocol.kt`.

## Requirements

- Android Studio Ladybug+ (AGP 8.7)
- Device/emulator with BLE (physical device recommended)
- minSdk **31** (Android 12) for `BLUETOOTH_SCAN` / `BLUETOOTH_CONNECT`
- `INTERNET` for preset HTTP execution
- `POST_NOTIFICATIONS` (Android 13+) for the link foreground service

## Build

```bash
cd companion
# Prefer Android Studio: Open this folder → Sync → Run 'app'
./gradlew :app:assembleDebug   # requires Android SDK (ANDROID_HOME or local.properties sdk.dir)
```

`local.properties` is gitignored. Example:

```
sdk.dir=/home/YOU/Android/Sdk
```

If the Gradle wrapper jar is missing, use Android Studio’s Gradle sync or install Gradle 8.9 and run `gradle wrapper`.

## Pairing / reconnect flow

1. Open Companion → allow Bluetooth **and notification** permissions → status **Listening…** (notification appears).
2. On Cardputer: **Settings → BLE → Pair** (first time) or **Connect** / open **Web**.
3. Phone connects when it sees advertising; `Hello OK — firmware …`
4. Screen lock / leave the app: service keeps listening (swipe notification **Disconnect** or in-app Disconnect to stop).
5. Device or phone **Disconnect** stops the link; tap **Scan** or reopen Companion to listen again.
6. **Forget** on device clears bond; use **Scan** if needed after re-pair.

## Web commander

1. Edit presets in the Companion list (defaults included).
2. Companion listening (UI or lock screen) → open **Web** on Cardputer.
3. Phone performs HTTPS; truncated body (≤512 B) is chunked over BLE to the device.
4. Leaving **Web** disconnects BLE on device; Companion resumes listening while the service runs.

See [docs/m2_verify.md](../docs/m2_verify.md).

## Permissions

- Denied Bluetooth / notification permissions → status explains what is needed.
- Bluetooth off → prompts to enable.
- Internet required to execute HTTP presets.
- Some OEMs still restrict background BLE; allow unrestricted battery for Companion if needed.
