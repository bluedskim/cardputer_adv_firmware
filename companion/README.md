# Cardputer Companion (Android)

BLE central for [cardputer_adv_firmware](../README.md): M1 link, **HTTP Web** presets, **SSH exec** presets, and **Find My Phone** (vibrate, optional sound).

Designed like a **smartwatch companion** (Mi Band–style):

1. **Link** once via Android Companion Device Manager
2. System **device presence** wakes the app when CardputerAdv advertises (lock screen / background)
3. **Battery saver** toggle (default **off**): when on, no idle wake lock and **LOW_POWER** scan (still finds commander ADV); when off, low-latency scan + wake lock for fastest reconnect
4. **Stop** — reliable mode keeps linked FGS standby; battery saver ends FGS (presence can restart). Long-press **Stop** → **Unlink**
5. **Scan** is a manual force-rescan fallback (no new association)

Also: HTTP and SSH presets on the phone; serves `HttpList*` / `HttpRun*`, `SshList*` / `SshRun*`, and `FindStart` / `FindStop` for the Cardputer **Web**, **SSH**, and **Find** apps. No notification mirror yet (M4). HTTP preset JSON is stored with encrypted preferences (a plain copy from older builds is migrated on next load).

**Test run** on each preset editor runs the request/SSH on the phone and shows body + step log (Copy log) before you rely on the Cardputer UI.

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
# or release (debug-signed until a store keystore is configured):
./gradlew :app:assembleRelease
```

GitHub Releases ship versioned assets, e.g. `CardputerCompanion-0.8.6.apk` next to `cardputer_adv_firmware-0.8.6.bin`.

`local.properties` example: `sdk.dir=/home/YOU/Android/Sdk`

## Watch-style setup

1. On Cardputer: **Settings → BLE → Pair** (first time), then leave advertising (Pair/Connect or open **Web** / **SSH**).
2. Open Companion → grant Bluetooth + notifications → tap **Link**.
3. System chooser: select **CardputerAdv** → association created → status **Linked — waiting…**
4. Allow **unrestricted battery** when prompted.
5. Optional: enable **Battery saver** to cut idle wake locks (commander connect may be slower).
6. Lock the phone. Open **Web** or **SSH** on Cardputer — phone should wake via presence and connect.
7. **Stop** / long-press **Unlink** / **Scan** as needed.

## Web commander

1. Edit HTTP presets in the Companion list (defaults included). Use **Test run** to verify on the phone.
2. Linked Companion → open **Web** on Cardputer.
3. Phone performs HTTPS; body ≤512 B chunked over BLE.
4. Leaving **Web** disconnects on device; phone returns to linked wait (or Stop).

See [docs/m2_verify.md](../docs/m2_verify.md).

## SSH commander

1. Add SSH presets: name, host, port, user, command, password (encrypted). Use **Test run** (TOFU host keys on first connect).
2. Open **SSH** on Cardputer → list / **Refresh hosts...** / run.
3. Phone runs password SSH exec via sshj; combined stdout/stderr ≤512 B over BLE.

See [docs/m3_verify.md](../docs/m3_verify.md).

## Permissions / OEM notes

- Companion permissions: run/use-data/start-FGS-from-background + observe presence
- Battery: Settings → Apps → Companion → Battery → Unrestricted
- Samsung/Xiaomi: also check Sleeping apps / Autostart if presence never fires
