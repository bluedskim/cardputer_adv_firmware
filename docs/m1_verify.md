# M1 verification checklist

## Simulator

```bash
pio run -e sim
```

- [x] Build succeeds with BLE stub
- [ ] Settings main shows `BLE N/A`
- [ ] Enter on BLE opens sub-screen with `(sim no BLE)`; Del returns

## Device firmware

```bash
pio run -e m5stack-cardputer -t upload
```

- [ ] Cold boot: main `BLE Off Unpaired` (or `Off Paired` after a prior bond)
- [ ] Enter BLE → **Pair** → status `Pairing`
- [ ] Phone companion Scan → connect → phone `Hello OK`; device `Connecting` then `Connected`
- [ ] **Disconnect** → radio off; main/summary `Off Paired`
- [ ] Display sleep then key wake stays on Settings (not Torch). GPIO light sleep only before the BLE stack has been started; after pair/connect the stack stays up and light sleep stays off even at `Off Paired`
- [ ] Leave connected ~60s without traffic → auto disconnect + `Off Paired`
- [ ] Reboot → `Off Paired` persists; BLE → **Connect** + companion Scan reconnects
- [ ] **Forget** → confirm → `Off Unpaired`; must Pair again
- [ ] G0 short opens Recorder and starts a take; short press while recording stops and saves

## Companion app

See [companion/README.md](../companion/README.md).

- [ ] Permission denial shows guidance
- [ ] Bluetooth off prompts enable
- [ ] Scan timeout message if device not advertising
