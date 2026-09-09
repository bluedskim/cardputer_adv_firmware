# M1 verification checklist

## Simulator

```bash
pio run -e sim
```

- [x] Build succeeds with BLE stub
- [ ] Settings shows `St N/A`
- [ ] BLE action row still navigable (actions no-op on sim)

## Device firmware

```bash
pio run -e m5launcher
# install dist/cardputer_adv_firmware.bin via M5Launcher
```

- [ ] Cold boot: Settings `St Off` (or `Paired` after a prior bond)
- [ ] **Pair:** Enter on BLE Pair → `St Pairing` / `Wait`
- [ ] Phone companion Scan → connect → `Hello OK` on phone; device `St Connected`
- [ ] **Disconnect** (phone or device) → radio off; `St Paired` or `Off`
- [ ] Display sleep then key wake still works with BLE off (light sleep path)
- [ ] Leave connected ~60s without traffic → auto disconnect + radio off
- [ ] Reboot → `Paired` persists; **Connect** + companion Scan reconnects
- [ ] **Forget** clears pair flag; must Pair again
- [ ] G0 still returns to M5Launcher

## Companion app

See [companion/README.md](../companion/README.md).

- [ ] Permission denial shows guidance
- [ ] Bluetooth off prompts enable
- [ ] Scan timeout message if device not advertising
