# Cardputer Companion (Android, M2)

BLE central for [cardputer_adv_firmware](../README.md): M1 link + **HTTP Web commander** presets.

- Scan for advertise name `CardputerAdv`
- Connect / bond, `Hello` / `HelloAck`
- HTTP presets on phone (name, URL, method, headers, body)
- Serves `HttpList*` / `HttpRun*` when the Cardputer **Web** app requests them
- No notification mirror or SSH yet (M3/M4)

Protocol: [docs/ble_link.md](../docs/ble_link.md). UUIDs must match `include/ble/ble_protocol.h` and `BleProtocol.kt`.

## Requirements

- Android Studio Ladybug+ (AGP 8.7)
- Device/emulator with BLE (physical device recommended)
- minSdk **31** (Android 12) for `BLUETOOTH_SCAN` / `BLUETOOTH_CONNECT`
- `INTERNET` for preset HTTP execution

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

## Pairing flow

1. On Cardputer: **Settings → BLE** (shows `Off Unpaired` or `Off Paired`) → **Enter** → **Pair** (or **Connect** if already paired).
2. Status becomes `Pairing`. On phone: Companion → allow Bluetooth permissions → **Scan**.
3. On success: phone shows `Hello OK — firmware …`; device shows `Connecting` then `Connected`.
4. **Disconnect** on the BLE screen (or phone). After idle (**60 s**) or disconnect, radio off → `Off Paired`.
5. **Forget** (with confirm) clears the bond → `Off Unpaired`.

## Web commander

1. Edit presets in the Companion list (defaults included).
2. On Cardputer open **Web** — device connects on demand, lists preset names, Enter runs one.
3. Phone performs HTTPS; truncated body (≤512 B) is chunked over BLE to the device.
4. Leaving **Web** disconnects BLE.

See [docs/m2_verify.md](../docs/m2_verify.md).

## Permissions

- Denied Bluetooth permissions → status explains what is needed.
- Bluetooth off → prompts to enable.
- Internet required to execute HTTP presets.
