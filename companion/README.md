# Cardputer Companion (Android, M1)

Minimal BLE central for [cardputer_adv_firmware](../README.md) M1 link.

- Scan for advertise name `CardputerAdv`
- Connect / bond
- Enable TX notifications, complete `Hello` / `HelloAck`
- No notification mirror, HTTP, or SSH (later milestones)

Protocol: [docs/ble_link.md](../docs/ble_link.md). UUIDs must match `include/ble/ble_protocol.h` and `BleProtocol.kt`.

## Requirements

- Android Studio Ladybug+ (AGP 8.7)
- Device/emulator with BLE (physical device recommended)
- minSdk **31** (Android 12) for `BLUETOOTH_SCAN` / `BLUETOOTH_CONNECT`

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

1. On Cardputer: **Settings → BLE → Pair** (Left/Right to choose action, Enter to run). Status should show `Pairing` / `Wait`.
2. On phone: open Companion → allow Bluetooth permissions → **Scan**.
3. On success: phone shows `Hello OK — firmware …`; device Status shows `Connected`.
4. **Disconnect** on either side. After idle (**60 s**) or disconnect, device radio turns **Off** (Status `Paired` if bonded).

## Permissions

- Denied Bluetooth permissions → status explains what is needed.
- Bluetooth off → prompts to enable.
