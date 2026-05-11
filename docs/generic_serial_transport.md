# Generic Serial Transport

The generic serial transport is ZCM’s default framing for byte-stream links (UART, USB serial, etc.). It provides:

- Byte framing and re-synchronization
- Channel + payload length fields
- Fletcher-16 checksum for basic corruption detection
- A simple escaping rule so the frame marker can appear in data

This transport is used by `serial://...` when neither `pkt_size` (packetized mode) nor `raw=true` are specified.

---

## Wire Format

Constants:

- Escape byte: `0xCC` (configurable via `ZCM_GENERIC_SERIAL_ESCAPE_CHAR`)
- Frame prefix: `0xCC 0x00`
- Checksum: Fletcher-16, seeded with `0xFFFF`

The unescaped frame layout is:

| Field | Size | Notes |
|---|---:|---|
| `0xCC 0x00` | 2 | Frame start |
| `chan_len` | 1 | Channel name length in bytes |
| `data_len` | 4 | Big-endian uint32 payload length |
| `chan` | `chan_len` | Channel bytes (not null-terminated on the wire) |
| `data` | `data_len` | Message bytes |
| `checksum` | 2 | Big-endian Fletcher-16 over `chan` then `data` (unescaped bytes) |

---

## Escaping Rules

To ensure the escape byte can appear in the channel name or message payload:

- Any literal `0xCC` byte in `chan` or `data` is transmitted as `0xCC 0xCC`.
- The receiver interprets:
  - `0xCC 0x00` as a frame start.
  - `0xCC 0xCC` as a single literal `0xCC` data byte.

The checksum is computed over the original (unescaped) `chan` and `data` bytes.

---

## Decoder Outline

At a high level, a receiver can:

1. Scan the byte stream for the two-byte prefix `0xCC 0x00`.
2. Read `chan_len` and `data_len` (big-endian).
3. Read `chan_len + data_len + 2` bytes, applying the unescaping rule while reading `chan` and `data`.
4. Compute Fletcher-16 over `chan` and `data` (seed `0xFFFF`) and compare to the transmitted checksum.
5. If the checksum matches, deliver the reconstructed `(channel, payload)` message.

---

## References

- Implementation: `zcm/transport/generic_serial_transport.c`
- Related: [Packetized Serial Transport](packetized_serial_transport.md)
