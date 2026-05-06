# Packetized Serial Transport

The packetized serial transport (`packetized_serial_transport.c`) is a framing layer that sits on top of the [generic serial transport](../zcm/transport/generic_serial_transport.h). It fragments large ZCM messages into bounded packet bodies that fit within the inner transport's MTU, and reassembles them on the receiver side. This makes it possible to send messages larger than the serial MTU over byte-stream links.

Messages are only packetized when their ZCM channel name begins with `$`. Packetized inner messages are sent on that same `$`-prefixed channel, and the `$` is stripped when the reassembled message is delivered. All other messages are forwarded directly to the inner transport unchanged, subject to the inner transport MTU.

---

## Packet Wire Format

Every packetized message sent over the inner transport has a fixed 4-byte header followed by a type-specific body. The `body_len` field is one byte, so a packetized body can be at most 255 bytes.

The main wire-format constants are:

- **`PACKETIZED_HEADER_BYTES` (4)** — the outer header present on every packet: `type` (1) + `session_id` (2) + `body_len` (1).
- **`PACKETIZED_MAX_BODY_BYTES` (255)** — the maximum body length represented by the one-byte `body_len` field.
- **`PACKETIZED_DATA_OVERHEAD_BYTES` (2)** — the non-payload prefix inside every DATA body: `packet_id` (2).
- **`PACKETIZED_MAX_PACKET_DATA_SIZE` (253)** — the maximum DATA payload bytes per packet: `PACKETIZED_MAX_BODY_BYTES - PACKETIZED_DATA_OVERHEAD_BYTES`.
- **`PACKETIZED_RETRANS_MAX_IDS` (127)** — the maximum packet IDs in one RETRANS_REQ body: `(255 - 1) / 2`.

The maximum usable payload per DATA packet is therefore `min(inner_mtu - PACKETIZED_HEADER_BYTES - PACKETIZED_DATA_OVERHEAD_BYTES, PACKETIZED_MAX_PACKET_DATA_SIZE)`. A configured payload size may choose any smaller valid value.

### Header (4 bytes, always present)

| Offset | Size | Field        | Description                              |
|--------|------|--------------|------------------------------------------|
| 0      | 1    | `type`       | Message type (see below)                 |
| 1–2    | 2    | `session_id` | Big-endian uint16; identifies a transfer |
| 3      | 1    | `body_len`   | Number of body bytes that follow         |

### METADATA body (9 bytes) — type = 1

Sent once at the start of each transfer. Contains everything the receiver needs to allocate state and validate the reassembled message. The checksum is Fletcher-16 over the full original message, seeded with `PACKETIZED_FLETCHER16_INIT` (`0xFFFF`).

| Offset | Size | Field                | Description                               |
|--------|------|----------------------|-------------------------------------------|
| 0–1    | 2    | `total_packets`      | Big-endian uint16; number of DATA packets |
| 2      | 1    | `tx_packet_data_size`| Payload bytes per DATA packet             |
| 3–6    | 4    | `total_message_size` | Big-endian uint32; full message bytes     |
| 7–8    | 2    | `checksum`           | Big-endian Fletcher-16 of the full message|

### DATA body (2 + N bytes) — type = 2

One packet per chunk of the message. `N` can be zero for an empty message; otherwise `N <= tx_packet_data_size`, and the last packet may be shorter than earlier packets.

| Offset | Size | Field       | Description                              |
|--------|------|-------------|------------------------------------------|
| 0–1    | 2    | `packet_id` | Big-endian uint16; zero-based index      |
| 2–...  | N    | `payload`   | Slice of the original message            |

### RETRANS_REQ body (1 + 2·count bytes) — type = 3

Sent by the receiver when it detects missing packets after a timeout (200 ms). The receiver splits requests so each RETRANS_REQ fits both the inner MTU and the 255-byte body limit. A zero `count` is accepted as a no-op, although the current receiver only emits requests with missing IDs.

| Offset | Size | Field     | Description                                  |
|--------|------|-----------|----------------------------------------------|
| 0      | 1    | `count`   | Number of packet IDs being requested         |
| 1–2    | 2    | `id[0]`   | Big-endian uint16 packet_id                  |
| 3–4    | 2    | `id[1]`   | …                                            |

---

## Validation Rules

Incoming packetized messages are parsed only on `$`-prefixed channels. Packets shorter than `PACKETIZED_HEADER_BYTES`, packets whose `body_len` does not match the received length, and unknown packet types are ignored.

METADATA packets are accepted only when:

- `body_len == PACKETIZED_METADATA_BODY_BYTES`.
- `total_packets > 0` and `packet_data_size > 0`.
- `packet_data_size <= PACKETIZED_MAX_PACKET_DATA_SIZE`.
- `total_message_size <= zt->mtu`.
- `total_packets == ceil(total_message_size / packet_data_size)`, with zero-length messages represented by one DATA packet.
- The receive buffers are already large enough, or dynamic growth succeeds.

DATA packets for inactive sessions or mismatched session IDs are ignored. DATA packets for the active session are invalid if the packet ID is out of range, if the body is shorter than the packet ID field, or if the payload length does not exactly match the expected chunk size for that packet. Duplicate DATA packets refresh the session timestamp, but do not rewrite data or increment `received_count`.

RETRANS_REQ packets for inactive sessions or mismatched session IDs are ignored. RETRANS_REQ bodies must be exactly `1 + 2 * count` bytes, and `count` must fit the transmitter's retransmission ID buffer. Requested packet IDs outside the active transmit session are skipped when retransmissions are sent.

`ZCM_EINVALID` from packet processing is treated as a dropped bad packet; the transport continues processing later packets.

## Data Structures

### `packetized_rx_state_t`

Tracks the state of one active receive session.

| Field               | Type       | Purpose                                                       |
|---------------------|------------|---------------------------------------------------------------|
| `active`            | `int`      | Non-zero while a session is being reassembled                 |
| `channel[]`         | `char`     | ZCM channel name for the in-progress message                  |
| `session_id`        | `uint16_t` | Session ID from METADATA; used to match DATA packets          |
| `total_packets`     | `uint16_t` | Expected number of DATA packets                               |
| `packet_data_size`  | `uint8_t`  | Payload bytes per packet (from METADATA)                      |
| `total_message_size`| `uint32_t` | Full message size in bytes                                    |
| `expected_checksum` | `uint16_t` | Fletcher-16 checksum to validate after reassembly             |
| `last_update_utime` | `uint64_t` | Timestamp of the last received DATA packet (retrans trigger)  |
| `data`              | `uint8_t*` | Reassembly buffer — payload slices are written here           |
| `packet_received[]` | `uint8_t*` | Per-packet arrival flag (1 = received, 0 = missing)           |
| `received_count`    | `uint16_t` | Number of DATA packets received so far                        |
| `retrans_pending`   | `int`      | A RETRANS_REQ needs to be sent on the next update             |
| `missing_ids[]`     | `uint16_t*`| List of packet IDs not yet received, built at retrans time    |
| `missing_count`     | `uint16_t` | Number of entries in `missing_ids`                            |
| `packet_capacity`   | `size_t`   | Allocated capacity for packet tracking arrays; kept in step with message buffer bytes |

### `packetized_tx_state_t`

Tracks the state of one active transmit session; kept alive to service retransmission requests.

| Field                | Type       | Purpose                                                      |
|----------------------|------------|--------------------------------------------------------------|
| `active`             | `int`      | Non-zero while the session is live (awaiting possible retrans)|
| `channel[]`          | `char`     | ZCM channel name of the outgoing message                     |
| `session_id`         | `uint16_t` | Session ID for this send; generated by incrementing `next_session_id` |
| `total_packets`      | `uint16_t` | Total DATA packets sent                                      |
| `packet_data_size`   | `uint8_t`  | Payload bytes per packet used for this session               |
| `total_message_size` | `uint32_t` | Full message size in bytes                                   |
| `data`               | `uint8_t*` | Copy of the sent message, held for retransmission            |
| `retrans_ids[]`      | `uint16_t*`| Packet IDs requested for retransmission by the receiver      |
| `retrans_count`      | `uint16_t` | Number of pending retransmission requests                    |
| `packet_capacity`    | `size_t`   | Allocated capacity for retransmit IDs; kept in step with message buffer bytes |

### `zcm_trans_packetized_serial_t`

The top-level transport object. Owns both the TX and RX state plus all shared resources.

| Field                        | Type                | Purpose                                                         |
|------------------------------|---------------------|-----------------------------------------------------------------|
| `trans`                      | `zcm_trans_t`       | The vtable pointer — satisfies the `zcm_trans_t` interface      |
| `inner`                      | `zcm_trans_t*`      | The underlying generic serial transport                         |
| `inner_mtu`                  | `size_t`            | MTU of the inner transport; sets the maximum packet size        |
| `mtu`                        | `size_t`            | Max message size this transport advertises to ZCM               |
| `max_message_size`           | `size_t`            | Current reassembly/transmit buffer capacity                     |
| `dynamic`                    | `int`               | If non-zero, buffers grow automatically to fit larger messages  |
| `configured_packet_data_size`| `uint8_t`           | Payload bytes per DATA packet (auto-selected or user-specified) |
| `pkt_buf` / `pkt_buf_size`   | `uint8_t*` / `size_t` | Scratch buffer for building one outbound inner-transport packet|
| `out_buf` / `out_buf_size`   | `uint8_t*` / `size_t` | One-slot output queue for pass-through or reassembled messages |
| `out_len` / `out_utime`      | `size_t` / `uint64_t` | Length and timestamp of the queued message                     |
| `out_channel[]`              | `char`              | Channel of the queued message; `$` prefix stripped after reassembly |
| `out_pending`                | `int`               | Non-zero if `out_buf` holds an undelivered message              |
| `next_session_id`            | `uint16_t`          | Incremented for each packetized send; wraps naturally           |
| `rx`                         | `packetized_rx_state_t` | Current receive session state                               |
| `tx`                         | `packetized_tx_state_t` | Current transmit session state                              |
| `time` / `time_usr`          | function pointer    | Callback for current timestamp in microseconds                  |

---

## Flowcharts

### Sending a Message

```
packetized_serial_sendmsg(msg)
         │
         ▼
send_pending_retransmissions()
send_retrans_request()
         │
         ▼
  channel starts with '$'?
    │              │
   No             Yes
    │              │
    ▼              ▼
send to inner   validate size and grow buffers if needed
transport       tx_packet_data_size = configured_packet_data_size
(pass-through)  total_packets = ceil(msg.len / tx_packet_data_size)
                zero-length messages still use one DATA packet
                     │
                     ▼
             copy msg to tx->data
             save active session in tx state
                     │
                     ▼
             Build METADATA packet:
             [total_packets | tx_packet_data_size |
              total_message_size | Fletcher-16 checksum]
             Send via inner transport
                     │
                     ▼
             For packet_id = 0 .. total_packets-1:
               slice = msg.buf[packet_id * chunk .. +chunk]
               Build DATA packet: [packet_id | slice]
               Send via inner transport
                     │
                     ▼
                  return ZCM_EOK
```

### Receiving a Message

```
packetized_serial_recvmsg()
         │
         ▼
  out_pending?  ──Yes──► deliver queued message, return ZCM_EOK
         │
        No
         │
         ▼
  process_incoming_messages()
         │
         ▼
  zcm_trans_recvmsg(inner)  ──EAGAIN──► maybe_schedule_retrans_request()
         │                                       │
      packet                             timeout elapsed &
       received                           packets missing?
         │                                       │
         ▼                                      Yes
  channel starts with '$'?                       │
    │              │                             ▼
   No             Yes                   build missing_ids list
    │              │                    set retrans_pending = 1
    ▼              ▼                             │
 queue output   Parse header:                    ▼
 (pass-through) type / session_id / body_len    send_retrans_request()
                          │                     send RETRANS_REQ packet(s)
          ┌───────────────┼────────────────┐
          ▼               ▼                ▼
       METADATA         DATA           RETRANS_REQ
          │               │                │
          ▼               ▼                ▼
   begin_rx_session  process_rx_data  process_retrans_request
          │               │           store retrans_ids in tx state
  validate params   validate exact        │
   grow buffers      payload length        ▼
   if needed         write new slice  (retransmissions sent on
   store session     into rx->data    next sendmsg or update_tx)
   state                  │
                    mark if new
                          │
                          ▼
               all packets received?
                 │           │
                No          Yes
                 │           │
                 │           ▼
                 │    validate Fletcher-16 checksum
                 │      pass?      fail?
                 │       │          │
                 │       ▼          ▼
                 │  queue_output  rx_clear
                 │  rx_clear      return EINVALID
                 │       │
                 └───────┘
                         │
                         ▼
                    deliver message
                    return ZCM_EOK
```

---

## Retransmission

When the receiver stops getting DATA packets mid-transfer, it detects the stall by comparing the current time against `rx->last_update_utime`. If the gap exceeds `PACKETIZED_RETRANS_TIMEOUT_US` (200 ms), it:

1. Builds the list of missing `packet_id` values into `rx->missing_ids`.
2. Sets `rx->retrans_pending = 1`.
3. On the next call to `send_retrans_request`, sends one or more RETRANS_REQ packets (splitting across multiple inner-MTU packets if there are many missing IDs).

`send_retrans_request` splits large missing-ID lists by the smaller of the inner MTU limit and `PACKETIZED_RETRANS_MAX_IDS`. On the transmitter side, `process_retrans_request` stores the requested IDs in `tx->retrans_ids`. They are sent by `send_pending_retransmissions`, which is called at the start of every `sendmsg` and `update_tx`.

---

## Buffer Sizing and Dynamic Growth

At creation time, `zcm_trans_packetized_serial_create` receives a `tx_packet_data_size` argument (0 = auto-select the largest value that fits in the inner MTU) and a `max_message_size` argument (0 = dynamic).

- The inner MTU must fit at least one DATA packet with one payload byte and one METADATA packet.
- If `tx_packet_data_size == 0`, the transport chooses the largest DATA payload that fits the inner MTU, capped at `PACKETIZED_MAX_PACKET_DATA_SIZE`.
- If `tx_packet_data_size > 0`, it must be no larger than `PACKETIZED_MAX_PACKET_DATA_SIZE` and `PACKETIZED_HEADER_BYTES + PACKETIZED_DATA_OVERHEAD_BYTES + tx_packet_data_size` must fit within the inner MTU.
- The advertised packetized MTU is `configured_packet_data_size * PACKETIZED_MAX_PACKETS`, capped to `max_message_size` in fixed mode.
- In **fixed** mode (`max_message_size > 0`), buffers are sized once at creation and packetized messages larger than `max_message_size` are rejected with `ZCM_EINVALID`.
- In **dynamic** mode (`max_message_size == 0`), buffers start at `PACKETIZED_DEFAULT_MAX_MESSAGE_SIZE` and `grow_buffers()` is called whenever an incoming METADATA packet or outgoing message exceeds the current allocation. The RX data, RX packet flags, RX missing IDs, TX data, TX retransmit IDs, and output buffer are grown as needed. If growth fails, the operation returns `ZCM_EAGAIN`.
