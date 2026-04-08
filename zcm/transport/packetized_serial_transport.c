#include "zcm/transport.h"
#include "zcm/zcm.h"

#include "generic_serial_transport.h"
#include "packetized_serial_protocol.h"
#include "packetized_serial_transport.h"

#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct packetized_rx_state_t packetized_rx_state_t;
struct packetized_rx_state_t
{
    int      active;
    char     channel[ZCM_CHANNEL_MAXLEN + 1];
    uint16_t session_id;
    uint16_t total_packets;
    uint8_t  packet_data_size;
    uint32_t total_message_size;
    uint32_t expected_crc;
    uint64_t last_update_utime;

    uint8_t* data;
    uint8_t* packet_received;
    uint16_t received_count;

    int       retrans_pending;
    uint16_t* missing_ids;
    uint16_t  missing_count;
    size_t    packet_capacity;
};

typedef struct packetized_tx_state_t packetized_tx_state_t;
struct packetized_tx_state_t
{
    int      active;
    char     channel[ZCM_CHANNEL_MAXLEN + 1];
    uint16_t session_id;
    uint16_t total_packets;
    uint8_t  packet_data_size;
    uint32_t total_message_size;
    uint8_t* data;

    uint16_t* retrans_ids;
    uint16_t  retrans_count;
    size_t    packet_capacity;
};

typedef struct zcm_trans_packetized_serial_t zcm_trans_packetized_serial_t;
struct zcm_trans_packetized_serial_t
{
    zcm_trans_t  trans;
    zcm_trans_t* inner;

    size_t  inner_mtu;
    size_t  mtu;
    size_t  max_message_size;
    uint8_t configured_packet_data_size;

    uint8_t* pkt_buf;
    size_t   pkt_buf_size;

    uint8_t* out_buf;
    size_t   out_len;
    uint64_t out_utime;
    char     out_channel[ZCM_CHANNEL_MAXLEN + 1];
    int      out_pending;

    uint16_t              next_session_id;
    packetized_rx_state_t rx;
    packetized_tx_state_t tx;

    uint64_t (*time)(void* usr);
    void* time_usr;
};

static zcm_trans_packetized_serial_t* cast(zcm_trans_t* zt);

static int send_inner_with_retry(zcm_trans_packetized_serial_t* zt, zcm_msg_t msg)
{
    int i;
    int ret = zcm_trans_sendmsg(zt->inner, msg);
    if (ret == ZCM_EOK) return ZCM_EOK;
    if (ret != ZCM_EAGAIN) return ret;

    for (i = 0; i < 4; ++i) {
        zcm_trans_update(zt->inner);
        ret = zcm_trans_sendmsg(zt->inner, msg);
        if (ret == ZCM_EOK) return ZCM_EOK;
        if (ret != ZCM_EAGAIN) return ret;
    }

    return ZCM_EAGAIN;
}

static int send_packet(zcm_trans_packetized_serial_t* zt, const char* channel,
                       uint16_t session_id, uint8_t type, const uint8_t* body,
                       uint8_t body_len)
{
    if (PACKETIZED_HEADER_BYTES + (size_t)body_len > zt->pkt_buf_size)
        return ZCM_EINVALID;

    zt->pkt_buf[0] = type;
    packetized_write_u16_be(&zt->pkt_buf[1], session_id);
    zt->pkt_buf[3] = body_len;
    if (body_len > 0 && body != &zt->pkt_buf[PACKETIZED_HEADER_BYTES]) {
        memcpy(&zt->pkt_buf[PACKETIZED_HEADER_BYTES], body, body_len);
    }

    zcm_msg_t out;
    out.utime   = 0;
    out.channel = channel;
    out.len     = PACKETIZED_HEADER_BYTES + body_len;
    out.buf     = zt->pkt_buf;
    return send_inner_with_retry(zt, out);
}

static void rx_clear(packetized_rx_state_t* rx)
{
    /* Resets all session state to idle. Allocated buffers (data, packet_received,
     * missing_ids) are kept intact and reused for the next session. */
    rx->active           = 0;
    rx->channel[0]       = '\0';
    rx->session_id       = 0;
    rx->total_packets    = 0;
    rx->packet_data_size = 0;
    rx->total_message_size = 0;
    rx->expected_crc       = 0;
    rx->last_update_utime  = 0;
    rx->received_count     = 0;
    rx->retrans_pending    = 0;
    rx->missing_count      = 0;
    if (rx->packet_received && rx->packet_capacity > 0)
        memset(rx->packet_received, 0, rx->packet_capacity);
}

static void tx_clear(packetized_tx_state_t* tx)
{
    /* Resets all session state to idle. Allocated buffers (data, retrans_ids)
     * are kept intact and reused for the next session. */
    tx->active             = 0;
    tx->channel[0]         = '\0';
    tx->session_id         = 0;
    tx->total_packets      = 0;
    tx->packet_data_size   = 0;
    tx->total_message_size = 0;
    tx->retrans_count      = 0;
}

static int is_packetized_channel(const char* channel)
{
    return channel && channel[0] == '$';
}

static int send_retrans_request(zcm_trans_packetized_serial_t* zt)
{
    size_t                 sent;
    packetized_rx_state_t* rx = &zt->rx;
    if (!rx->retrans_pending || !rx->active || rx->missing_count == 0) return ZCM_EOK;

    size_t max_ids_per_req = (zt->inner_mtu > PACKETIZED_HEADER_BYTES + 1)
                                 ? (zt->inner_mtu - PACKETIZED_HEADER_BYTES - 1) / 2
                                 : 0;
    if (max_ids_per_req == 0) return ZCM_EINVALID;

    sent = 0;
    while (sent < rx->missing_count) {
        size_t   i;
        size_t   remaining = rx->missing_count - sent;
        size_t   count     = remaining < max_ids_per_req ? remaining : max_ids_per_req;
        size_t   body_len  = 1 + count * 2;
        uint8_t* body      = zt->pkt_buf + PACKETIZED_HEADER_BYTES;
        body[0]            = (uint8_t)count;
        for (i = 0; i < count; ++i) {
            packetized_write_u16_be(&body[1 + i * 2], rx->missing_ids[sent + i]);
        }

        int ret = send_packet(zt, rx->channel, rx->session_id, PACKETIZED_MSG_RETRANS_REQ,
                              body, (uint8_t)body_len);
        if (ret != ZCM_EOK) return ret;
        sent += count;
    }

    rx->retrans_pending = 0;
    return ZCM_EOK;
}

static int queue_output(zcm_trans_packetized_serial_t* zt, const char* channel,
                        const uint8_t* data, size_t len, uint64_t utime,
                        int strip_packetized_prefix)
{
    if (zt->max_message_size < len) {
        return ZCM_EINVALID;
    }

    if (len > 0) memcpy(zt->out_buf, data, len);

    if (strip_packetized_prefix && channel[0] == '$') channel += 1;
    strncpy(zt->out_channel, channel, ZCM_CHANNEL_MAXLEN);
    zt->out_channel[ZCM_CHANNEL_MAXLEN] = '\0';
    zt->out_len                         = len;
    zt->out_utime                       = utime;
    zt->out_pending                     = 1;
    return ZCM_EOK;
}

static void deliver_pending(zcm_trans_packetized_serial_t* zt, zcm_msg_t* msg)
{
    msg->utime      = zt->out_utime;
    msg->channel    = zt->out_channel;
    msg->len        = zt->out_len;
    msg->buf        = zt->out_buf;
    zt->out_pending = 0;
}

static size_t packetized_max_mtu(uint8_t packet_data_size)
{
    return (size_t)packet_data_size * PACKETIZED_MAX_PACKETS;
}

static size_t packet_capacity_for(size_t max_message_size, uint8_t packet_data_size)
{
    size_t packet_count = packetized_message_packet_count(max_message_size, packet_data_size);
    return packet_count == 0 ? 1 : packet_count;
}

static int send_pending_retransmissions(zcm_trans_packetized_serial_t* zt)
{
    size_t                 i;
    packetized_tx_state_t* tx = &zt->tx;
    if (!tx->active || tx->retrans_count == 0) return ZCM_EOK;

    size_t   chunk = tx->packet_data_size;
    uint8_t* body  = zt->pkt_buf + PACKETIZED_HEADER_BYTES;

    for (i = 0; i < tx->retrans_count; ++i) {
        uint16_t packet_id = tx->retrans_ids[i];
        if (packet_id >= tx->total_packets) continue;

        uint32_t offset      = (uint32_t)packet_id * (uint32_t)chunk;
        uint32_t remaining   = tx->total_message_size - offset;
        uint8_t  payload_len = (uint8_t)(remaining < chunk ? remaining : chunk);

        packetized_write_u16_be(&body[0], packet_id);
        if (payload_len > 0) memcpy(&body[2], tx->data + offset, payload_len);

        int ret = send_packet(zt, tx->channel, tx->session_id, PACKETIZED_MSG_DATA, body,
                              (uint8_t)(PACKETIZED_DATA_OVERHEAD_BYTES + payload_len));
        if (ret != ZCM_EOK) return ret;
    }

    tx->retrans_count = 0;
    return ZCM_EOK;
}

static int begin_rx_session(zcm_trans_packetized_serial_t* zt, const char* channel,
                            uint16_t session_id, const uint8_t* body, size_t body_len,
                            uint64_t utime)
{
    if (body_len != PACKETIZED_METADATA_BODY_BYTES) return ZCM_EINVALID;

    uint16_t total_packets      = packetized_read_u16_be(&body[0]);
    uint8_t  packet_data_size   = body[2];
    uint32_t total_message_size = packetized_read_u32_be(&body[3]);
    uint32_t expected_crc       = packetized_read_u32_be(&body[7]);

    if (total_packets == 0 || packet_data_size == 0) return ZCM_EINVALID;
    if (packet_data_size > 253) return ZCM_EINVALID;
    if (total_message_size > zt->max_message_size || total_message_size > zt->mtu)
        return ZCM_EINVALID;

    size_t expected_packets = packetized_message_packet_count(total_message_size,
                                                              packet_data_size);
    if (expected_packets != total_packets) return ZCM_EINVALID;

    rx_clear(&zt->rx);
    packetized_rx_state_t* rx = &zt->rx;
    if ((size_t)total_packets > rx->packet_capacity) return ZCM_EINVALID;

    strncpy(rx->channel, channel, ZCM_CHANNEL_MAXLEN);
    rx->channel[ZCM_CHANNEL_MAXLEN] = '\0';
    rx->active                      = 1;
    rx->session_id                  = session_id;
    rx->total_packets               = total_packets;
    rx->packet_data_size            = packet_data_size;
    rx->total_message_size          = total_message_size;
    rx->expected_crc                = expected_crc;
    rx->last_update_utime           = utime;
    return ZCM_EOK;
}

static int process_rx_data(zcm_trans_packetized_serial_t* zt, uint16_t session_id,
                           const uint8_t* body, size_t body_len, uint64_t utime)
{
    packetized_rx_state_t* rx = &zt->rx;
    if (!rx->active || rx->session_id != session_id) return ZCM_EOK;
    if (body_len < PACKETIZED_DATA_OVERHEAD_BYTES) return ZCM_EINVALID;

    uint16_t packet_id   = packetized_read_u16_be(&body[0]);
    uint8_t  payload_len = (uint8_t)(body_len - PACKETIZED_DATA_OVERHEAD_BYTES);

    if (packet_id >= rx->total_packets) return ZCM_EINVALID;

    uint32_t offset = (uint32_t)packet_id * (uint32_t)rx->packet_data_size;
    if (offset > rx->total_message_size) return ZCM_EINVALID;

    uint32_t remaining = rx->total_message_size - offset;
    uint8_t  expected_len =
        (uint8_t)(remaining < rx->packet_data_size ? remaining : rx->packet_data_size);
    if (payload_len != expected_len) return ZCM_EINVALID;

    if (!rx->packet_received[packet_id]) {
        if (payload_len > 0) memcpy(rx->data + offset, &body[2], payload_len);
        rx->packet_received[packet_id] = 1;
        ++rx->received_count;
    }

    rx->last_update_utime = utime;

    if (rx->received_count == rx->total_packets) {
        uint32_t crc = packetized_crc32_compute(rx->data, rx->total_message_size);
        if (crc != rx->expected_crc) {
            rx_clear(rx);
            return ZCM_EINVALID;
        }

        int ret = queue_output(zt, rx->channel, rx->data, rx->total_message_size, utime, 1);
        if (ret != ZCM_EOK) {
            rx_clear(rx);
            return ret;
        }

        rx_clear(rx);
    }

    return ZCM_EOK;
}

static int process_retrans_request(zcm_trans_packetized_serial_t* zt, uint16_t session_id,
                                   const uint8_t* body, size_t body_len)
{
    uint8_t                i;
    packetized_tx_state_t* tx = &zt->tx;
    if (!tx->active || tx->session_id != session_id) return ZCM_EOK;
    if (body_len < 1) return ZCM_EINVALID;

    uint8_t count = body[0];
    if ((size_t)(1 + (size_t)count * 2) != body_len) return ZCM_EINVALID;

    tx->retrans_count = 0;
    if (count == 0) return ZCM_EOK;

    if ((size_t)count > tx->packet_capacity) return ZCM_EINVALID;
    tx->retrans_count = count;
    for (i = 0; i < count; ++i) {
        tx->retrans_ids[i] = packetized_read_u16_be(&body[1 + (size_t)i * 2]);
    }

    return ZCM_EOK;
}

static void maybe_schedule_retrans_request(zcm_trans_packetized_serial_t* zt,
                                           uint64_t                       now)
{
    uint16_t               i;
    packetized_rx_state_t* rx = &zt->rx;
    if (!rx->active || rx->retrans_pending) return;
    if (rx->received_count == rx->total_packets) return;
    if (now <= rx->last_update_utime) return;
    if (now - rx->last_update_utime < PACKETIZED_RETRANS_TIMEOUT_US) return;

    uint16_t missing_count = (uint16_t)(rx->total_packets - rx->received_count);
    if (missing_count == 0) return;

    if ((size_t)missing_count > rx->packet_capacity) return;

    uint16_t idx = 0;
    for (i = 0; i < rx->total_packets; ++i) {
        if (!rx->packet_received[i]) rx->missing_ids[idx++] = i;
    }
    rx->missing_count   = idx;
    rx->retrans_pending = idx > 0;
}

static size_t packetized_serial_get_mtu(zcm_trans_packetized_serial_t* zt) { return zt->mtu; }

static int packetized_serial_sendmsg(zcm_trans_packetized_serial_t* zt, zcm_msg_t msg)
{
    uint16_t packet_id;
    size_t   chan_len = strlen(msg.channel);
    if (chan_len > ZCM_CHANNEL_MAXLEN) return ZCM_EINVALID;

    int ret = send_pending_retransmissions(zt);
    if (ret != ZCM_EOK) return ret;
    ret = send_retrans_request(zt);
    if (ret != ZCM_EOK) return ret;

    if (!is_packetized_channel(msg.channel)) {
        if (msg.len > zt->inner_mtu) return ZCM_EINVALID;
        return send_inner_with_retry(zt, msg);
    }

    uint8_t packet_data_size = zt->configured_packet_data_size;
    if (packet_data_size == 0 || packet_data_size > PACKETIZED_MAX_PACKET_DATA_SIZE)
        return ZCM_EINVALID;
    if (msg.len > zt->mtu || msg.len > zt->max_message_size) return ZCM_EINVALID;

    uint32_t total_message_size = (uint32_t)msg.len;
    size_t total_packets_sz = packetized_message_packet_count(total_message_size,
                                                              packet_data_size);
    if (total_packets_sz == 0) total_packets_sz = 1;
    if (total_packets_sz > PACKETIZED_MAX_PACKETS) return ZCM_EINVALID;
    uint16_t total_packets = (uint16_t)total_packets_sz;

    tx_clear(&zt->tx);
    packetized_tx_state_t* tx = &zt->tx;
    if (msg.len > 0) memcpy(tx->data, msg.buf, msg.len);

    tx->active             = 1;
    tx->session_id         = ++zt->next_session_id;
    tx->packet_data_size   = packet_data_size;
    tx->total_packets      = total_packets;
    tx->total_message_size = total_message_size;
    strncpy(tx->channel, msg.channel, ZCM_CHANNEL_MAXLEN);
    tx->channel[ZCM_CHANNEL_MAXLEN] = '\0';

    uint8_t* meta = zt->pkt_buf + PACKETIZED_HEADER_BYTES;
    packetized_write_u16_be(&meta[0], total_packets);
    meta[2] = packet_data_size;
    packetized_write_u32_be(&meta[3], total_message_size);
    packetized_write_u32_be(&meta[7], packetized_crc32_compute(msg.buf, msg.len));
    ret = send_packet(zt, tx->channel, tx->session_id, PACKETIZED_MSG_METADATA, meta,
                      PACKETIZED_METADATA_BODY_BYTES);
    if (ret != ZCM_EOK) return ret;

    uint8_t* body = zt->pkt_buf + PACKETIZED_HEADER_BYTES;
    for (packet_id = 0; packet_id < total_packets; ++packet_id) {
        uint32_t offset    = (uint32_t)packet_id * (uint32_t)packet_data_size;
        uint32_t remaining = total_message_size - offset;
        uint8_t  payload_len =
            (uint8_t)(remaining < packet_data_size ? remaining : packet_data_size);

        packetized_write_u16_be(&body[0], packet_id);
        if (payload_len > 0) memcpy(&body[2], tx->data + offset, payload_len);

        ret = send_packet(zt, tx->channel, tx->session_id, PACKETIZED_MSG_DATA, body,
                          (uint8_t)(PACKETIZED_DATA_OVERHEAD_BYTES + payload_len));
        if (ret != ZCM_EOK) return ret;
    }

    return ZCM_EOK;
}

static int packetized_serial_recvmsg_enable(zcm_trans_packetized_serial_t* zt,
                                     const char* channel, bool enable)
{
    return zcm_trans_recvmsg_enable(zt->inner, channel, enable);
}

static int process_incoming_messages(zcm_trans_packetized_serial_t* zt)
{
    while (!zt->out_pending) {
        zcm_msg_t in;
        int       ret = zcm_trans_recvmsg(zt->inner, &in, 0);
        if (ret != ZCM_EOK) return ret;

        if (!is_packetized_channel(in.channel)) {
            return queue_output(zt, in.channel, in.buf, in.len, in.utime, 0);
        }

        if (in.len < PACKETIZED_HEADER_BYTES) continue;
        {
            uint8_t  type       = in.buf[0];
            uint16_t session_id = packetized_read_u16_be(&in.buf[1]);
            uint8_t  body_len   = in.buf[3];
            if (in.len != PACKETIZED_HEADER_BYTES + body_len) continue;

            const uint8_t* body = &in.buf[PACKETIZED_HEADER_BYTES];
            uint64_t       now  = in.utime == 0 ? zt->time(zt->time_usr) : in.utime;

            if (type == PACKETIZED_MSG_METADATA) {
                ret = begin_rx_session(zt, in.channel, session_id, body, body_len, now);
            } else if (type == PACKETIZED_MSG_DATA) {
                ret = process_rx_data(zt, session_id, body, body_len, now);
            } else if (type == PACKETIZED_MSG_RETRANS_REQ) {
                ret = process_retrans_request(zt, session_id, body, body_len);
            } else {
                continue;
            }

            if (ret != ZCM_EOK && ret != ZCM_EINVALID) return ret;
        }
    }

    return ZCM_EOK;
}

static int packetized_serial_recvmsg(zcm_trans_packetized_serial_t* zt, zcm_msg_t* msg,
                              unsigned timeoutMs)
{
    (void)timeoutMs;

    if (zt->out_pending) {
        deliver_pending(zt, msg);
        return ZCM_EOK;
    }

    while (1) {
        int ret = process_incoming_messages(zt);
        if (ret == ZCM_EAGAIN) {
            maybe_schedule_retrans_request(zt, zt->time(zt->time_usr));
            ret = send_retrans_request(zt);
            if (ret != ZCM_EOK) return ret;
            return ZCM_EAGAIN;
        }
        if (ret != ZCM_EOK) return ret;

        deliver_pending(zt, msg);
        return ZCM_EOK;
    }
}

int packetized_serial_update_rx(zcm_trans_t* _zt)
{
    zcm_trans_packetized_serial_t* zt = cast(_zt);
    int ret = serial_update_rx(zt->inner);
    if (ret != ZCM_EOK) return ret;

    ret = process_incoming_messages(zt);
    if (ret != ZCM_EOK && ret != ZCM_EAGAIN) return ret;

    maybe_schedule_retrans_request(zt, zt->time(zt->time_usr));
    return send_retrans_request(zt);
}

int packetized_serial_update_tx(zcm_trans_t* _zt)
{
    zcm_trans_packetized_serial_t* zt = cast(_zt);
    int ret = send_pending_retransmissions(zt);
    if (ret != ZCM_EOK) return ret;
    return serial_update_tx(zt->inner);
}

static size_t _packetized_serial_get_mtu(zcm_trans_t* zt)
{
    return packetized_serial_get_mtu(cast(zt));
}

static int _packetized_serial_sendmsg(zcm_trans_t* zt, zcm_msg_t msg)
{
    return packetized_serial_sendmsg(cast(zt), msg);
}

static int _packetized_serial_recvmsg_enable(zcm_trans_t* zt, const char* channel,
                                             bool enable)
{
    return packetized_serial_recvmsg_enable(cast(zt), channel, enable);
}

static int _packetized_serial_recvmsg(zcm_trans_t* zt, zcm_msg_t* msg, unsigned timeoutMs)
{
    return packetized_serial_recvmsg(cast(zt), msg, timeoutMs);
}

static int _packetized_serial_update(zcm_trans_t* zt)
{
    int rxRet = packetized_serial_update_rx(zt);
    int txRet = packetized_serial_update_tx(zt);
    return rxRet == ZCM_EOK ? txRet : rxRet;
}

static zcm_trans_methods_t methods = {
    &_packetized_serial_get_mtu,
    &_packetized_serial_sendmsg,
    &_packetized_serial_recvmsg_enable,
    &_packetized_serial_recvmsg,
    NULL,
    &_packetized_serial_update,
    &zcm_trans_packetized_serial_destroy,
};

static zcm_trans_packetized_serial_t* cast(zcm_trans_t* zt)
{
    assert(zt->vtbl == &methods);
    return (zcm_trans_packetized_serial_t*)zt;
}

zcm_trans_t* zcm_trans_packetized_serial_create(
    size_t (*get)(uint8_t* data, size_t nData, void* usr),
    size_t (*put)(const uint8_t* data, size_t nData, void* usr), void* put_get_usr,
    uint64_t (*timestamp_now)(void* usr), void* time_usr, size_t MTU, size_t bufSize,
    uint8_t packet_data_size, size_t max_message_size)
{
    zcm_trans_packetized_serial_t* zt = calloc(1, sizeof(*zt));
    if (zt == NULL) return NULL;

    zt->inner = zcm_trans_generic_serial_create(get, put, put_get_usr, timestamp_now,
                                                time_usr, MTU, bufSize);
    if (zt->inner == NULL) {
        free(zt);
        return NULL;
    }

    zt->inner_mtu = zcm_trans_get_mtu(zt->inner);
    if (zt->inner_mtu < PACKETIZED_HEADER_BYTES + PACKETIZED_DATA_OVERHEAD_BYTES + 1 ||
        zt->inner_mtu < PACKETIZED_HEADER_BYTES + PACKETIZED_METADATA_BODY_BYTES) {
        zcm_trans_generic_serial_destroy(zt->inner);
        free(zt);
        return NULL;
    }

    if (max_message_size == 0) max_message_size = PACKETIZED_DEFAULT_MAX_MESSAGE_SIZE;
    zt->max_message_size = max_message_size;

    if (packet_data_size == 0) {
        size_t max_payload =
            zt->inner_mtu - PACKETIZED_HEADER_BYTES - PACKETIZED_DATA_OVERHEAD_BYTES;
        if (max_payload > PACKETIZED_MAX_PACKET_DATA_SIZE)
            max_payload = PACKETIZED_MAX_PACKET_DATA_SIZE;
        zt->configured_packet_data_size = (uint8_t)max_payload;
    } else {
        zt->configured_packet_data_size = packet_data_size;
    }

    if (zt->configured_packet_data_size == 0 ||
        zt->configured_packet_data_size > PACKETIZED_MAX_PACKET_DATA_SIZE) {
        zcm_trans_generic_serial_destroy(zt->inner);
        free(zt);
        return NULL;
    }

    if (PACKETIZED_HEADER_BYTES + PACKETIZED_DATA_OVERHEAD_BYTES +
            zt->configured_packet_data_size >
        zt->inner_mtu) {
        zcm_trans_generic_serial_destroy(zt->inner);
        free(zt);
        return NULL;
    }

    zt->mtu = packetized_max_mtu(zt->configured_packet_data_size);
    if (zt->mtu > zt->max_message_size) zt->mtu = zt->max_message_size;

    zt->pkt_buf_size = zt->inner_mtu;
    zt->pkt_buf      = malloc(zt->pkt_buf_size);
    if (zt->pkt_buf == NULL) {
        zcm_trans_generic_serial_destroy(zt->inner);
        free(zt);
        return NULL;
    }

    zt->out_buf = malloc(zt->max_message_size == 0 ? 1 : zt->max_message_size);
    if (zt->out_buf == NULL) {
        free(zt->pkt_buf);
        zcm_trans_generic_serial_destroy(zt->inner);
        free(zt);
        return NULL;
    }
    zt->out_len        = 0;
    zt->out_utime      = 0;
    zt->out_channel[0] = '\0';
    zt->out_pending    = 0;

    zt->rx.packet_capacity = packet_capacity_for(zt->max_message_size,
                                                 zt->configured_packet_data_size);
    zt->rx.data = malloc(zt->max_message_size == 0 ? 1 : zt->max_message_size);
    zt->rx.packet_received = calloc(zt->rx.packet_capacity, sizeof(uint8_t));
    zt->rx.missing_ids = malloc(zt->rx.packet_capacity * sizeof(uint16_t));
    zt->tx.data = malloc(zt->max_message_size == 0 ? 1 : zt->max_message_size);
    zt->tx.packet_capacity = zt->rx.packet_capacity;
    zt->tx.retrans_ids = malloc(zt->tx.packet_capacity * sizeof(uint16_t));
    if (zt->rx.data == NULL || zt->rx.packet_received == NULL || zt->rx.missing_ids == NULL ||
        zt->tx.data == NULL || zt->tx.retrans_ids == NULL) {
        free(zt->tx.retrans_ids);
        free(zt->tx.data);
        free(zt->rx.missing_ids);
        free(zt->rx.packet_received);
        free(zt->rx.data);
        free(zt->out_buf);
        free(zt->pkt_buf);
        zcm_trans_generic_serial_destroy(zt->inner);
        free(zt);
        return NULL;
    }
    rx_clear(&zt->rx);
    tx_clear(&zt->tx);

    zt->time     = timestamp_now;
    zt->time_usr = time_usr;

    zt->trans.trans_type = ZCM_NONBLOCKING;
    zt->trans.vtbl       = &methods;
    return (zcm_trans_t*)zt;
}

void zcm_trans_packetized_serial_destroy(zcm_trans_t* _zt)
{
    zcm_trans_packetized_serial_t* zt = cast(_zt);
    if (zt->inner) zcm_trans_generic_serial_destroy(zt->inner);
    free(zt->pkt_buf);
    free(zt->out_buf);
    free(zt->rx.missing_ids);
    free(zt->rx.packet_received);
    free(zt->rx.data);
    free(zt->tx.retrans_ids);
    free(zt->tx.data);
    free(zt);
}
