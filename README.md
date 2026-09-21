# cardputer_adv_firmware

Current release: **[v0.6.0](https://github.com/bluedskim/cardputer_adv_firmware/releases/tag/v0.6.0)** · Companion `0.6.2-m3` (M3 SSH)

Custom firmware for the M5Stack **Cardputer-Adv**. Switch apps left/right like the stock UI. Includes **Torch**, **Timer**, **Stopwatch**, **Web**, **SSH**, and **Settings**.

Primary development target is **Fedora**; setup and flash steps below prefer Fedora, with notes for other platforms.

## Screenshots

### Cardputer firmware

<p align="center">
  <img src="docs/images/firmware-torch.png" alt="Torch app on Cardputer" width="320" />
  <img src="docs/images/firmware-web.png" alt="Web commander list on Cardputer" width="320" />
  <img src="docs/images/firmware-ssh.png" alt="SSH command result on Cardputer" width="320" />
</p>

| Torch | Web list | SSH result |
| :---: | :---: | :---: |
| Brightness / timeout flashlight | Pick an HTTP preset | Exit status + stdout paging |

### Android Companion

<p align="center">
  <img src="docs/images/companion-main.png" alt="Companion main screen with HTTP and SSH presets" width="240" />
  &nbsp;&nbsp;
  <img src="docs/images/companion-ssh-dialog.png" alt="Companion SSH preset editor with Test run" width="240" />
</p>

| Main | SSH preset |
| :---: | :---: |
| Link / Scan / Stop, HTTP & SSH lists | Edit host + **Test run** before flash |

## Development environment (Fedora)

### 1. System packages

```bash
sudo dnf install git python3-pip gcc-c++ make SDL2-devel pkg-config
```

| Package | Purpose |
| -------------------------- | ------------------ |
| `git`, `python3-pip` | PlatformIO, cloning |
| `gcc-c++`, `make` | Native / sim builds |
| `SDL2-devel`, `pkg-config` | Desktop simulator (`sim`) |

### 2. PlatformIO

**Cursor / VS Code extension** (recommended): install [PlatformIO IDE](https://platformio.org/install/ide?install=vscode), open the repo, and let it fetch dependencies from `platformio.ini`.

**CLI only:**

```bash
pip install --user platformio
# Add ~/.local/bin to PATH if needed (e.g. ~/.bashrc)
export PATH="$HOME/.local/bin:$PATH"
pio --version
```

On the first build from the repo root, the ESP32 toolchain and libraries download under `~/.platformio`.

### 3. USB upload and serial monitor

Cardputer-Adv (ESP32-S3) appears as USB CDC on `/dev/ttyACM*`. Without permission, `pio run -t upload` or `pio device monitor` fails.

**Option A —** `dialout` **group (typical)**

```bash
sudo usermod -aG dialout "$USER"
# Log out and back in (or reboot)
```

**Option B — udev rule (Espressif)**

`/etc/udev/rules.d/99-espressif.rules`:

```
SUBSYSTEM=="usb", ATTR{idVendor}=="303a", MODE="0666", GROUP="dialout"
```

Apply:

```bash
sudo udevadm control --reload-rules
sudo udevadm trigger
# Replug the USB cable
```

Check the port:

```bash
pio device list
```

### 4. Other Linux / macOS / Windows

| OS | Notes |
| ------------- | ----------------------------------------------------------------------------------------------------------------------------------------- |
| Debian/Ubuntu | `sudo apt install build-essential git python3-pip libsdl2-dev pkg-config` · serial group `dialout` |
| macOS | [PlatformIO IDE](https://platformio.org/install/ide?install=vscode) · sim: `brew install sdl2 pkg-config` ([sim/README.md](sim/README.md)) |
| Windows | PlatformIO IDE · sim: MSYS2 + SDL2 ([sim/README.md](sim/README.md)) |

## UI

Shared **two-level arrow navigation**. Prefer arrows, Enter, and Del over letter shortcuts.

- Top chrome bar: app name, page dots, battery
- **G0** returns to M5Launcher

### Arrow keys

Use the arrow glyphs printed on the keycaps. **Fn + arrow** works the same way.

| Key | Direction |
| ------------- | --- |
| `;` or Fn+`;` | Up |
| `,` or Fn+`,` | Left |
| `.` or Fn+`.` | Down |
| `/` or Fn+`/` | Right |

### Common controls

| Action | Keys |
| ------------ | ----------------------------------- |
| Move selection | `↑` `↓` |
| Adjust value / no submenu | `←` `→` (app-dependent) |
| Select / run | `Enter` |
| Back | `←` or `Del` |
| Switch apps | From the **top** of a list, `↑` → chrome focus → `←` `→` |
| Return to app | From chrome, `↓` or `Enter` |

When chrome is focused, the bar turns navy and the title yellow.

## Features

### Torch

Full-brightness backlight as a flashlight.

| Action | Keys |
| ------------------------- | ----------------- |
| Focus Bright / Time / Blink | `↑` `↓` |
| Adjust brightness or timeout; toggle Blink | `←` `→` |
| Toggle Blink (when focused) | `Enter` |
| Switch apps | From Bright, `↑` → chrome |

- Timeout step: **60 s** (max **60 min**)
- Default timeout: **10 min (600 s)**
- When the timeout expires, the light turns off and can turn on again (any key restarts the timer)
- Long high-brightness use stresses the panel — keep a timeout set

### Timer

Multiple countdown timers stored in NVS. Set **hours and minutes** only.

**List**

- `↑` `↓` — timer / `+ New`
- `Enter` — open / start / clear done / add New
- While running, `Enter` — cancel
- Top row `↑` — chrome (app switch)

**Edit**

- `↑` `↓` — Hours / Minutes / `> Start` / `x Remove`
- `←` `→` — adjust hours/minutes (0–23 h, 0–59 m)
- `Enter` — Start or Remove (confirm)
- `←` / `Del` — back to list

Finished timers show `DONE` in the list.

### Stopwatch

Named timing sessions stored in NVS (max **10**).

| Item | `Enter` |
| ------------ | ----------------------- |
| Name | Edit name |
| Start / Stop | Start or stop (Stop saves history) |
| History | Saved runs |

- `↑` `↓` — move · top `↑` → chrome
- History: `↑` `↓` scroll · `←` / `Del` back

### Web

HTTP commander over BLE (phone executes presets). Cache-first name list; trailing **Refresh sites...** forces a sync. Enter runs a preset; result shows status + body paging.

See [docs/m2_verify.md](docs/m2_verify.md) · [docs/ble_link.md](docs/ble_link.md).

### SSH

SSH exec-only commander over BLE (phone holds host/user/password/command). Cache-first list; **Refresh hosts...**; result shows exit status + stdout/stderr paging.

See [docs/m3_verify.md](docs/m3_verify.md) · [docs/ble_link.md](docs/ble_link.md).

### Settings

Last app in the order. Sleep, brightness, BLE link, battery, and firmware version.

- `↑` `↓` — Sleep / Bright / BLE · from Sleep, `↑` → chrome
- `←` `→` — Sleep: **Immediate** / 30 s / 1 min / **5 min** / 10 min / **Never** · Bright: 32 / 64 / **128** / 192 / 255
  - Immediate: screen off soon. **On sleep**, Sleep is rewritten to **30 sec** and saved (pick Immediate again to reuse)
- **BLE row:** status summary (`Off Unpaired` / `Off Paired` / `Pairing` / `Connecting` / `Connected` · sim: `N/A`). `Enter` → BLE submenu
- **BLE submenu**
  - Shows status and peer (when known)
  - Contextual actions only: unpaired `Pair` · paired `Connect` / `Forget` · in progress `Cancel` · connected `Disconnect`
  - Forget asks `Forget?` (`Enter` yes / `Del` cancel)
  - `←` / `Del` / top `↑` → main Settings (BLE row focused)
- **Battery:** percent and voltage on one line (charging shown on the chrome battery icon)
- **S/W Ver:** firmware version (`kFirmwareVersion` in `config.h`)
- With no input, backlight turns off after Sleep. If no Timer/Stopwatch is running **and** BLE radio is **off**, the device enters light sleep and wakes only on **TCA8418 INT (GPIO11)**. While BLE Pair/Connect or linked, light sleep is skipped; after **60 s** of idle link traffic the device disconnects and powers the radio down.
- Torch uses its own brightness; leaving Torch restores Settings Bright
- Android companion: [`companion/`](companion/) · protocol: [`docs/ble_link.md`](docs/ble_link.md) · verify: [`docs/m1_verify.md`](docs/m1_verify.md)

## Desktop simulator

Preview Torch / Timer / Stopwatch / Web / SSH / Settings on a PC with `sim/`. On Fedora, install the **system packages** above (`SDL2-devel`, `pkg-config`, …), then:

```bash
pio run -e sim -t upload
```

The `upload` target opens an SDL window. Keys, layout, and limits: [sim/README.md](sim/README.md).

## Build

### For M5Launcher (recommended)

Flash [M5Launcher](https://github.com/bmorcelli/Launcher) on the device first, then install this project as an **OTA app binary**.

```bash
pio run -e m5launcher
```

Outputs:

| File | Purpose |
| ------------------------------------ | --------------------- |
| `dist/cardputer_adv_firmware.bin` | App image for M5Launcher |
| `.pio/build/m5launcher/firmware.bin` | Same file (build path) |

**Install**

1. In M5Launcher, install the `.bin` via `SD` / `OTA` / `WUI`
2. Run the app from the launcher list

**Return to launcher**

- Press **G0** once (upper-left of the keyboard) → reboot to M5Launcher (factory)
- Recovery: hold **G0** and press **RST** → force launcher menu

> Flashing `firmware.bin` to `0x0` with `esptool` erases M5Launcher. Always use M5Launcher Install.

GitHub Release assets also include **`CardputerCompanion.apk`** (sideload; debug-signed until a store keystore is added).

### Device test checklist

After installing the `.bin` with M5Launcher (sim has no backlight hardware):

- [ ] **Brightness:** In Torch, focus Bright — `←` dims, `→` brightens; at max, `→` stays at 255
- [ ] **Timeout:** Focus Time — `←` `→` step by 60 s; countdown decreases each second; after expiry a message appears and keys work again
- [ ] **Timer:** Add with `+ New`, set Hours/Minutes only, Start → countdown / DONE
- [ ] **Stopwatch:** Rename, Start/Stop, History (max 10)
- [ ] **Settings:** Change/save Sleep & Bright; BLE summary + submenu (Pair/Connect/Cancel/Disconnect/Forget); battery %/V; firmware version; idle backlight off and key wake (same app); light sleep when no timer and BLE off
- [ ] **BLE M1:** BLE → Pair → companion Scan → Connected → Disconnect → `Off Paired` · sleep/wake keeps app · Forget confirm. See [companion/README.md](companion/README.md) · [docs/m1_verify.md](docs/m1_verify.md)
- [ ] **Web M2 / SSH M3:** Linked Companion → open Web or SSH → list / Refresh / run. [docs/m2_verify.md](docs/m2_verify.md) · [docs/m3_verify.md](docs/m3_verify.md)
- [ ] **G0:** While the app runs, G0 once → back to M5Launcher
- [ ] **Battery:** Chrome icon visible; lightning when USB is plugged (no CHG pin — USB presence is used as a hint)

### USB direct flash (standalone firmware)

Only when not using M5Launcher. On Fedora, fix [USB permissions](#3-usb-upload-and-serial-monitor) first:

```bash
pio run -e m5stack-cardputer
pio run -e m5stack-cardputer -t upload
pio device monitor
```

If upload fails: use a **data** cable, confirm the port in `pio device list`, re-login after adding `dialout`, and try bootloader mode (**RST**, etc.) on ESP32-S3.

### Requirements

- Fedora: packages in [Development environment](#development-environment-fedora) + [PlatformIO](https://platformio.org/)
- **Cardputer-Adv** (ESP32-S3, TCA8418 keyboard)
- USB-C **data** cable

M5Cardputer is declared in `platformio.ini`.

## Project layout

```
.
├── platformio.ini
├── scripts/export_launcher_bin.py
├── dist/                     # M5Launcher .bin (after build)
├── docs/                     # ble_link.md · m1/m2/m3 verify · images/
├── companion/                # Android companion (M1–M3)
├── sim/                      # Desktop simulator (SDL2, Arduino/M5 stubs)
├── include/
│   ├── config.h
│   ├── system_settings.h     # Sleep/Bright (NVS)
│   └── ble/                  # Protocol constants · BleLink API
├── src/
│   ├── main.cpp
│   ├── app_shell.*           # App switcher · sleep/wake
│   ├── system_settings.cpp
│   ├── launcher_exit.*       # Return to M5Launcher
│   ├── ble/ble_link.cpp      # NimBLE link (sim stub)
│   ├── input/                # Key edge · arrow mapping
│   ├── ui/                   # Battery icon
│   └── apps/
│       ├── torch/
│       ├── timer/
│       ├── stopwatch/
│       ├── web/              # HTTP commander (M2)
│       ├── ssh/              # SSH commander (M3)
│       └── settings/
└── README.md
```

## Configuration

Tune values in `include/config.h`:

| Constant | Default | Meaning |
| ------------------------------- | ---- | ---------------------------------- |
| `kTorchDefaultTimeoutSec` | 600 | Torch default timeout (s) |
| `kTorchMaxTimeoutSec` | 3600 | Torch timeout cap (s) |
| `kMaxTimers` | 8 | Max stored timers |
| `kTimerMaxHours` | 23 | Timer hours cap |
| `kMaxStopwatchHistory` | 10 | Max stopwatch history entries |
| `kDefaultSleepOptionIndex` | 3 | Default Sleep (5 min; includes Immediate / Never) |
| `kDefaultBrightnessOptionIndex` | 2 | Default Bright (128) |
| `kBleIdleDisconnectSec` | 60 | BLE idle → radio off (s) |

## Roadmap

- [ ] Features
  - [x] Sleep
  - [ ] ~~Battery charge threshold~~ (hardware unsupported)
  - [ ] Settings backup
  - [ ] Hangul text output

- [ ] More utility apps
  - [ ] Voice recorder
  - [ ] Expression calculator
  - [ ] SD reader/writer
  - [ ] Metronome
  - [ ] Tuner (six guitar-string pitches)
  - [x] Timer
  - [x] Stopwatch
  - [ ] Keyboard HID (Bluetooth, USB)
  - [ ] Clock
- [ ] SD card backup / restore

### Next major track — phone-backed companion

Theme: Cardputer is a thin terminal UI; an Android phone owns networking, secrets, and background work.
Cardputer Wi‑Fi is unused. Device↔phone link is **BLE** by default.

Goal: keep offline-util standby life near current light-sleep levels.
Always-on notification mirroring costs more battery; **on-demand commanders** cost less.

#### Shared (prerequisite for all companion features)

- [x] BLE pairing, reconnect, connection UI (Settings)
- [x] Link protocol: frame types, request/response IDs, error codes, payload limits ([docs/ble_link.md](docs/ble_link.md))
- [x] Secrets only in phone secure storage (no SSH keys/tokens in Cardputer NVS)
- [x] Sleep / battery policy
  - [x] **BLE off by default** — paired but radio off for standby life
  - [x] **Commander on-demand** — auto Connect for Web/SSH; disconnect on app exit / idle *(Settings Connect optional)*
  - [ ] **Notifications opt-in** — Settings toggle only; when off, fully disconnect *(M4)*
  - [ ] With notifications ON, keep existing display sleep; buffer quietly and refresh lists after wake (forced bright wake optional) *(M4)*
- [x] Out of scope: iOS companion, Cardputer-native Wi‑Fi/SSH stack, full web browser

#### M1 — Companion app (connectivity)

- [x] Android companion ([`companion/`](companion/) · connect / permissions)
- [x] Pairing · default BLE off / connection status
- [x] Out of scope: notification / commander UI (later milestones)

#### M2 — Web commander (HTTP templates)

- [x] Phone presets: name, URL, method, headers/body slots
- [x] Select on Cardputer → run → page response body
- [x] Short messages for timeout, HTTP errors, disconnect
- [x] On-demand link only (preserve standby life)
- [x] Out of scope: arbitrary URL browsing, JS render, cookie journal UI

Verify: [docs/m2_verify.md](docs/m2_verify.md) · protocol: [docs/ble_link.md](docs/ble_link.md)

#### M3 — SSH commander (exec-only)

- [x] Phone host presets: host, user, password (secure storage; key auth out of scope for v1)
- [x] Non-interactive exec: one-line command → stdout/stderr paging
- [x] known_hosts / first-connect handling on companion (TOFU)
- [x] On-demand link only (preserve standby life)
- [x] Out of scope: interactive PTY, port-forward, SFTP, multi-session tmux

Verify: [docs/m3_verify.md](docs/m3_verify.md) · protocol: [docs/ble_link.md](docs/ble_link.md)

#### M4 — Notification mirror (opt-in, higher standby cost)

- [ ] Settings notification mirror ON/OFF (default OFF)
- [ ] Notification access + read-only mirror (app / title / summary)
- [ ] Cardputer notification list + detail (scroll, `Del` back)
- [ ] No forced screen wake on receive (buffer; manual check; bright wake optional)
- [ ] Out of scope: notification actions (reply/dismiss), media controls, calls

Priority: **shared → M1 → M2 → M3 → M4**.
Ship commanders (M2/M3) before notifications (M4) to protect standby battery.
Keep calculator / metronome / tuner / HID / clock as offline utilities, separate from this major track.

## Debugging

1. [ ] After wake from sleep, UI seemed to always open Torch — should restore the screen that was active at sleep

## License

Unless stated otherwise, licensing follows the repository owner’s policy.
