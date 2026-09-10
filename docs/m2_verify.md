# M2 verify — Web commander (HTTP templates)

Prerequisites: M1 pairing works ([m1_verify.md](m1_verify.md)). Companion build ≥ `0.3.0-m2`. Firmware ≥ `0.5.3-dev` with **Web** app.

## Companion presets

1. Open Companion → **HTTP presets** list (defaults: `ipify`, `httpbin get`).
2. **Add** / tap to edit: name (≤20), URL, method, optional headers (`Key: Value` per line), body.
3. Long-press → delete. Max **12** presets. Stored on phone only (SharedPreferences).

## Auto-reconnect + lock screen

1. Open Companion (grant BT + **notification** permissions). Status **Listening…**; ongoing notification appears.
2. On Cardputer: Connect or open **Web** — phone connects without Scan.
3. **Lock the phone screen** → notification still shows Listening/Connected; Cardputer Web should still connect/run.
4. Leave Web / disconnect on device → Companion service resumes Listening.
5. In-app **Disconnect** or notification **Disconnect** stops the service.
6. **Scan** forces a fresh search (stuck link / re-pair).

## Device flow

1. Settings → BLE → **Pair** once if needed. **Settings → Connect is not required** for Web.
2. Open **Web**. Footer shows link status; site names stay in the list (cached offline after first successful list).
3. Enter on a preset → run → site name + HTTP status in footer + body.
4. Del / Left from result → list. Leave Web → device disconnects; phone keeps listening while service runs.

## Error cases

| Case | Expect |
|------|--------|
| Not paired | Footer `Pair in Settings` |
| Companion killed / no notification service | Footer `No phone` / list timeout; start Companion |
| Empty preset list | `No presets` (or cached names if previously downloaded) |
| Bad URL / network fail | Footer HTTP err |
| Leave Web during run | Disconnect; no crash |

## Out of scope (do not file as bugs)

Arbitrary URL entry on device, JS render, cookies UI, cleartext HTTP, OEM battery savers that still kill FGS (allow unrestricted battery for Companion).
