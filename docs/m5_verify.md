# M5 verify — Find My Phone

Requires a paired Android Companion (`0.6.2-m3` or a build that includes Find handlers) and firmware with the **Find** app.

1. Settings → BLE paired and phone Companion running (background OK).
2. Open **Find**. Default row **Sound Off**.
3. Focus **Find phone** → `Enter`.
   - Footer `Connecting...` then `Finding`.
   - Phone **vibrates** and shows a Find notification. No ringtone.
4. `Enter` or `Del` → phone stops; footer `Stopped`.
5. Set **Sound On** (`←`/`→` or `Enter` on the Sound row) → `Enter` again.
   - Phone vibrates **and** plays the alarm/ringtone.
6. Tap notification **Stop**, or wait **60 s** → alert ends (`Stopped` / `Timed out`).
7. Leave Find → BLE disconnects (on-demand). Web/SSH still work afterward.
8. Unpaired: footer `Pair phone in Settings`; no crash.
