# M3 verify — SSH commander (exec-only)

Prerequisites: M2 Web link works ([m2_verify.md](m2_verify.md)). Companion build ≥ `0.6.0-m3`. Firmware ≥ `0.6.0-dev` with **SSH** app.

## Companion presets

1. Open Companion → **SSH presets** (below HTTP presets).
2. **Add** / tap to edit: name (≤20), host, port (default 22), user, command, password.
3. Password is stored in EncryptedSharedPreferences; other fields in normal prefs. Max **12**.
4. Long-press → delete.

## Device flow

1. Linked Companion (same as M2). Settings → BLE → Pair once if needed.
2. Open **SSH**. Cache-first list (auto List only if empty). Trailing row **Refresh hosts...** forces sync.
3. Enter on a host → phone runs SSH exec (password auth) → footer `exit N` + combined stdout/stderr (≤512 bytes), paging like Web. Phone shows a result notification (success includes body text).
4. Del / Left from result → list. Leave SSH → device disconnects.

## Error cases

| Case | Expect |
|------|--------|
| Empty SSH preset list | `No hosts on phone` (or cached names) |
| Bad password | Footer `SSH auth failed` / SSH err |
| Host key change after TOFU | Footer `SSH host key error` |
| Unreachable / timeout | `SSH timed out` / `SSH failed` |
| Leave SSH during run | Disconnect; no crash |

## Out of scope (do not file as bugs)

Interactive PTY, port-forward, SFTP, key/file auth, typing commands on Cardputer, multi-session.
