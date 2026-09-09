# M2 verify — Web commander (HTTP templates)

Prerequisites: M1 pairing works ([m1_verify.md](m1_verify.md)). Companion build ≥ `0.2.0-m2`. Firmware ≥ `0.5.0-dev` with **Web** app.

## Companion presets

1. Open Companion → **HTTP presets** list (defaults: `ipify`, `httpbin get`).
2. **Add** / tap to edit: name (≤20), URL, method, optional headers (`Key: Value` per line), body.
3. Long-press → delete. Max **12** presets. Stored on phone only (SharedPreferences).

## Device flow

1. Settings → BLE → pair once if needed → leave radio as needed.
2. Open **Web** (chrome Left/Right). Device should **Connect** on enter.
3. After Hello: status **Loading…** then preset list (names from phone).
4. Enter on a preset → **Running…** → result header `HTTP nnn` + body (≤512 bytes, scroll Up/Down).
5. Del / Left from result → list. Leave Web app → disconnect (on-demand).

## Error cases

| Case | Expect |
|------|--------|
| Not paired | `Pair in Settings` |
| Phone off / no connect | `Connect timeout` (~15s); Enter retries |
| Empty preset list | `No presets` |
| Bad URL / network fail | Result `Err 16` (`HttpFailed`) or status line |
| Slow host | `Err 17` / HTTP timeout path |
| Leave Web during run | Disconnect; no crash |

## Out of scope (do not file as bugs)

Arbitrary URL entry on device, JS render, cookies UI, cleartext HTTP (companion `usesCleartextTraffic=false` unless you change it).
