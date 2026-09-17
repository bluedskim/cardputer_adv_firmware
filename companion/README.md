# Cardputer Companion (Android, M2)

BLE central for [cardputer_adv_firmware](../README.md): M1 link + **HTTP Web commander** presets.

Designed like a **smartwatch companion** (Mi Band–style):

1. **Link** once via Android Companion Device Manager
2. System **device presence** wakes the app when CardputerAdv advertises (lock screen / background)
3. **Battery saver** toggle (default **off**): when on, no idle wake lock and **LOW_POWER** scan (still finds Web ADV); when off, low-latency scan + wake lock for fastest reconnect
4. **Stop** — reliable mode keeps linked FGS standby; battery saver ends FGS (presence can restart). Long-press **Stop** → **Unlink**
5. **Scan** is a manual force-rescan fallback (no new association)

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
5. Optional: enable **Battery saver** to cut idle wake locks (Web connect may be slower).
6. Lock the phone. Open **Web** on Cardputer — phone should wake via presence and connect.
7. **Stop** / long-press **Unlink** / **Scan** as needed.

## Web commander

1. Edit presets in the Companion list (defaults included).
2. Linked Companion (UI, lock screen, or presence-woken) → open **Web** on Cardputer.
3. Phone performs HTTPS; body ≤512 B chunked over BLE.
4. Leaving **Web** disconnects on device; phone returns to linked wait (or Stop).

See [docs/m2_verify.md](../docs/m2_verify.md).

## Permissions / OEM notes

- Companion permissions: run/use-data/start-FGS-from-background + observe presence
- Battery: Settings → Apps → Companion → Battery → Unrestricted
- Samsung/Xiaomi: also check Sleeping apps / Autostart if presence never fires
