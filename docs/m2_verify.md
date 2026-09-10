# M2 verify — Web commander (HTTP templates)

Prerequisites: M1 pairing works ([m1_verify.md](m1_verify.md)). Companion build ≥ `0.2.1-m2`. Firmware ≥ `0.5.0-dev` with **Web** app.

## Companion presets

1. Open Companion → **HTTP presets** list (defaults: `ipify`, `httpbin get`).
2. **Add** / tap to edit: name (≤20), URL, method, optional headers (`Key: Value` per line), body.
3. Long-press → delete. Max **12** presets. Stored on phone only (SharedPreferences).

## Auto-reconnect (foreground)

1. Open Companion (grant BT permissions once). Status should be **Listening…** without tapping Scan.
2. On Cardputer: Connect or open **Web** — phone should connect on its own.
3. Leave Web / disconnect → Companion (still open) returns to Listening and can connect again.
4. Put Companion in background → listening stops (battery). Bring to foreground → Listening resumes.
5. **Scan** is only needed to force a fresh search (first pair stuck, wrong peer, etc.).

## Device flow

1. Settings → BLE → **Pair** once if needed. **Settings → Connect is not required** for Web.
2. Open **Web** (chrome Left/Right). Device shows **Connecting…** and advertises on its own, then **Loading…** after Hello.
3. Preset list appears (names from phone).
4. Enter on a preset → if link dropped, auto-reconnect then **Running…** → `HTTP nnn` + body (≤512 bytes, scroll Up/Down).
5. Del / Left from result → list. Leave Web app → disconnect (on-demand).

## Error cases

| Case | Expect |
|------|--------|
| Not paired | `Pair in Settings` |
| Companion not open / backgrounded | `Connect timeout` (~15s); open Companion and retry |
| Empty preset list | `No presets` |
| Bad URL / network fail | Result `Err 16` (`HttpFailed`) or status line |
| Slow host | `Err 17` / HTTP timeout path |
| Leave Web during run | Disconnect; no crash |

## Out of scope (do not file as bugs)

Arbitrary URL entry on device, JS render, cookies UI, cleartext HTTP (companion `usesCleartextTraffic=false` unless you change it), background BLE service while app is killed.
