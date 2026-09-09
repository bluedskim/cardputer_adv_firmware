# BLE link protocol (Cardputer ↔ Android companion)

Phone-backed companion link. Cardputer is a **GATT peripheral**; the phone is the **central**.
Cardputer WiFi is not used. Secrets (SSH keys, tokens) stay on the phone only.

## Power policy

| Mode | Radio | Light sleep |
|------|-------|-------------|
| Default / after disconnect | **Off** | Allowed (TCA8418 INT wake) |
| Pairing / Connect advertising | On | Blocked |
| Connected | On | Blocked until idle disconnect |
| Paired, radio off | Off | Allowed |

M1 idle disconnect: after **60 s** with an open connection and no frame traffic, firmware disconnects and turns the radio off.

Notification mirroring (M4) is opt-in and separate; M1 does not keep a persistent link.

## Device

| Field | Value |
|-------|-------|
| Advertise name | `CardputerAdv` |
| Role | Peripheral (NimBLE) |

## GATT UUIDs

| Role | UUID |
|------|------|
| Service | `c7a1f001-5b2e-4a1d-9c3f-0d1e2f3a4b5c` |
| RX (phone → device, Write / Write Without Response) | `c7a1f002-5b2e-4a1d-9c3f-0d1e2f3a4b5c` |
| TX (device → phone, Notify) | `c7a1f003-5b2e-4a1d-9c3f-0d1e2f3a4b5c` |

Keep these identical in:

- `include/ble/ble_protocol.h`
- `companion/app/src/main/java/.../BleProtocol.kt`

## Frame format

Little-endian. Max ATT payload should fit a single notification (keep frames small; **no PSRAM**).

| Offset | Size | Field |
|--------|------|-------|
| 0 | 1 | `type` |
| 1 | 2 | `req_id` |
| 3 | 2 | `len` (payload bytes) |
| 5 | `len` | `payload` |

Constants:

- Header size: **5**
- Max payload: **48**
- Max frame: **53**

## Message types (M1)

| `type` | Name | Direction | Payload |
|--------|------|-----------|---------|
| `0x01` | `Hello` | Device → Phone | UTF-8 firmware version (no NUL required; `len` is authoritative) |
| `0x02` | `HelloAck` | Phone → Device | UTF-8 companion app version |
| `0x03` | `Ping` | Either | empty or opaque echo bytes |
| `0x04` | `Pong` | Either | same as Ping payload |
| `0x7F` | `Error` | Either | 1-byte error code |

### Error codes

| Code | Meaning |
|------|---------|
| `0x01` | Bad frame / overflow |
| `0x02` | Unknown type |
| `0x03` | Not ready |
| `0x10` | HTTP failed |
| `0x11` | HTTP timeout |
| `0x12` | BLE not ready |
| `0x13` | No presets |

### Handshake

1. Phone connects (and bonds on first pair).
2. Phone discovers GATT, enables TX notifications (CCCD).
3. Device sends `Hello` (`req_id` increments) **after** notify is subscribed.
4. Phone replies `HelloAck` with the same `req_id`.
5. Either side may `Ping`/`Pong` afterward.

## Message types (M2 HTTP commander)

Phone holds presets (name, URL, method, optional headers/body). Device lists and runs them; response body is chunked (max **512** bytes shown on device).

| `type` | Name | Direction | Payload |
|--------|------|-----------|---------|
| `0x10` | `HttpListReq` | Device → Phone | empty |
| `0x11` | `HttpListItem` | Phone → Device | `index(1)` `total(1)` UTF-8 name (rest of payload) |
| `0x12` | `HttpListEnd` | Phone → Device | empty |
| `0x13` | `HttpRunReq` | Device → Phone | `index(1)` |
| `0x14` | `HttpRunMeta` | Phone → Device | `http_status_u16_le(2)` `err_u8(1)` `reserved(1)` |
| `0x15` | `HttpRunChunk` | Phone → Device | `seq_u8(1)` + body bytes |
| `0x16` | `HttpRunEnd` | Phone → Device | empty |

`HttpRunMeta.err`: `0` = OK (use `http_status`); otherwise an `ErrorCode` (`HttpFailed` / `HttpTimeout` / …). Chunks are ordered `seq = 0..n-1`. Truncate body to 512 bytes on the phone before sending.

On-demand: Web Commander opens → device `Connect` → list/run → exit app → `disconnect` (idle 60s still applies while connected).

## Reserved (M3+)

| Range | Intent |
|-------|--------|
| `0x20`–`0x2F` | SSH commander |
| `0x30`–`0x3F` | Notification mirror |

## NVS on device (no secrets)

Namespace `card_adv_cfg` (shared with system settings):

| Key | Meaning |
|-----|---------|
| `ble_paired` | `u8` — bonded / paired flag for UI |
| `ble_peer` | short string (≤16) — last peer label for display only |

Bonding material stays in the BLE stack / phone OS. Do not store SSH keys or HTTP tokens in NVS.
