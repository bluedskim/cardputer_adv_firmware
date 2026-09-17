# M2 verify — Web commander (HTTP templates)

Prerequisites: M1 pairing works ([m1_verify.md](m1_verify.md)). Companion build ≥ `0.4.4-m2`. Firmware ≥ `0.5.6-dev` with **Web** app.

## Companion presets

1. Open Companion → **HTTP presets** list (defaults: `ipify`, `httpbin get`).
2. **Add** / tap to edit: name (≤20), URL, method, optional headers (`Key: Value` per line), body.
3. Long-press → delete. Max **12** presets. Stored on phone only (SharedPreferences).

## Watch-style link + lock screen

1. Cardputer advertising (BLE Pair/Connect or Web). Phone: Companion → grant BT + notifications.
2. Tap **Link** → system chooser → select **CardputerAdv**. Status **Linked — waiting…**; presence observation starts.
   - Phone **Location** must be ON. If the list is empty: flash firmware ≥ `0.5.4-dev`, keep Connect/Web advertising, or **Scan** once then **Link** again.
3. Allow **unrestricted battery** when prompted.
4. **Lock the phone** for several minutes. Open **Web** on Cardputer — phone should connect without unlocking (presence → FGS → GATT).
5. Leave Web → phone returns to waiting (association kept).
6. **Stop** ends FGS only; long-press **Stop** → **Unlink** clears companion association.
7. **Scan** = manual force search (fallback if presence is slow).

Companion ≥ `0.4.4-m2`: while **Linked**, **Stop** keeps standby listening (session drop only) so Web site-select still works. Firmware ≥ `0.5.5-dev` uses a stable public BLE address.

## Device flow

1. Settings → BLE → **Pair** once if needed. **Settings → Connect is not required** for Web.
2. Open **Web**. If a site list was cached, it shows immediately (no auto BLE sync). Empty cache triggers list download. Last row **Refresh sites...** forces a manual sync. Enter on a site runs it (connects on demand).
3. Enter on a preset → run → site name + HTTP status in footer + body.
4. Del / Left from result → list. Leave Web → device disconnects; phone waits via presence while linked.

## Error cases

| Case | Expect |
|------|--------|
| Not linked (no CDM association) | Tap **Link** while Cardputer advertises |
| Not paired | Footer `Pair in Settings` |
| Companion unlinked / killed | Footer `No phone` / list timeout; Link again |
| Empty preset list | `No presets` (or cached names if previously downloaded) |
| Bad URL / network fail | Footer HTTP err |
| Leave Web during run | Disconnect; no crash |

## Out of scope (do not file as bugs)

Arbitrary URL entry on device, JS render, cookies UI, cleartext HTTP, OEM battery savers that still block companion presence (allow unrestricted battery; check Autostart).
