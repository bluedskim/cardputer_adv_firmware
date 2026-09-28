# Desktop Simulator (`sim/`)

Preview Torch / Timer / Stopwatch / Recorder / Web / SSH / Find / Settings in an SDL2 window on a PC. Real keyboard hardware is stubbed. Recorder has no mic or SD. Find has no phone.

For PlatformIO install, USB permissions, and shared setup, follow the **Fedora** section in the root [README.md](../README.md#development-environment-fedora).

## Extra packages (SDL2)

The root README Fedora `dnf install` already covers `SDL2-devel` and `pkg-config` for sim builds. Other OSes:

### Linux (Debian/Ubuntu)

```bash
sudo apt install build-essential libsdl2-dev pkg-config
```

### macOS

```bash
brew install sdl2 pkg-config
```

To use the `native_arm` environment in `platformio.ini`, add macOS-specific flags as needed.

### Windows

Requires [MSYS2 UCRT64](https://www.msys2.org/) plus SDL2 development packages. See the [M5GFX PlatformIO_SDL guide](https://github.com/m5stack/M5GFX/tree/master/examples/PlatformIO_SDL).

## Run

From the repository root:

```bash
pio run -e sim -t upload
```

The `upload` target launches the simulator binary on the PC.

Build only:

```bash
pio run -e sim
.pio/build/sim/program
```

## Keys

Same two-level navigation as the device:

| Key | Action |
|----|------|
| `↑` `↓` (`;` `.`) | Move selection |
| `←` `→` (`,` `/`) | Adjust values (Torch) / back (`←`) |
| `Enter` | Select / run |
| `Del` | Back or delete character |
| Top of list `↑` | Header focus → `←` `→` to switch apps (device also accepts Fn+arrows) |
| Header `↓` / `Enter` | Return to app |
| `` ` `` short | G0 short — Recorder hotkey (start, or stop & save), same as the device |
| `` ` `` hold (~0.5 s) | G0 long — prints a launcher-reboot message (no `esp_restart` in sim) |

## Layout

```
sim/
  include/            # Arduino / M5Cardputer / Preferences stubs
  src/                # SDL main, keyboard polling
src/apps/             # Real app code (reused by the sim build)
```

## Limits

- M5Launcher / NVS persistence are in-memory stubs
- BLE is stubbed (`BLE N/A`); the submenu opens but actions are no-ops
- Always re-check on hardware ([docs/m1_verify.md](../docs/m1_verify.md))
