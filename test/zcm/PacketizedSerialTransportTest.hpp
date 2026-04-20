#ifndef PACKETIZED_SERIAL_TRANSPORT_TEST_HPP
#define PACKETIZED_SERIAL_TRANSPORT_TEST_HPP

#include "cxxtest/TestSuite.h"
#include "zcm/transport/packetized_serial_transport.h"
#include "zcm/transport/packetized_serial_protocol.h"

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

using std::string;
using std::vector;

struct PacketizedLinkEndpoint
{
    vector<uint8_t>         rx;
    vector<uint8_t>         txParse;
    PacketizedLinkEndpoint* peer{ nullptr };

    bool     dropEnabled{ false };
    bool     dropDone{ false };
    uint16_t dropPacketId{ 0 };
    string   dropChannel;
};

// decode_frame parses one generic_serial framing layer frame from buf[start..].
// The outer frame format is: 0xCC 0x00 chan_len data_len(4B) *chan *data checksum(2B),
// with 0xCC bytes in chan/data escaped as 0xCC 0xCC.
// NOTE: This mirrors the private framing in generic_serial_transport.c. If that
// framing changes, this function must be updated accordingly.
static bool decode_frame(const vector<uint8_t>& buf, size_t start, size_t& frame_end,
                         string& channel, vector<uint8_t>& data)
{
    if (buf.size() < start + 7) return false;
    if (buf[start] != 0xcc || buf[start + 1] != 0x00) return false;

    size_t   cur      = start + 2;
    uint8_t  chan_len = buf[cur++];
    uint32_t data_len = ((uint32_t)buf[cur] << 24) | ((uint32_t)buf[cur + 1] << 16) |
                        ((uint32_t)buf[cur + 2] << 8) | (uint32_t)buf[cur + 3];
    cur += 4;

    channel.clear();
    channel.reserve(chan_len);
    for (uint8_t i = 0; i < chan_len; ++i) {
        if (cur >= buf.size()) return false;
        uint8_t c = buf[cur++];
        if (c == 0xcc) {
            if (cur >= buf.size()) return false;
            uint8_t c2 = buf[cur++];
            if (c2 != 0xcc) return false;
            c = c2;
        }
        channel.push_back((char)c);
    }

    data.clear();
    data.reserve(data_len);
    for (uint32_t i = 0; i < data_len; ++i) {
        if (cur >= buf.size()) return false;
        uint8_t c = buf[cur++];
        if (c == 0xcc) {
            if (cur >= buf.size()) return false;
            uint8_t c2 = buf[cur++];
            if (c2 != 0xcc) return false;
            c = c2;
        }
        data.push_back(c);
    }

    if (cur + 2 > buf.size()) return false;
    frame_end = cur + 2;
    return true;
}

static size_t endpoint_get(uint8_t* data, size_t nData, void* usr)
{
    auto*  ep = (PacketizedLinkEndpoint*)usr;
    size_t n  = ep->rx.size() < nData ? ep->rx.size() : nData;
    if (n == 0) return 0;
    memcpy(data, ep->rx.data(), n);
    ep->rx.erase(ep->rx.begin(), ep->rx.begin() + (ptrdiff_t)n);
    return n;
}

static size_t endpoint_put(const uint8_t* data, size_t nData, void* usr)
{
    auto* ep = (PacketizedLinkEndpoint*)usr;
    if (nData == 0) return 0;

    ep->txParse.insert(ep->txParse.end(), data, data + nData);

    size_t consumed = 0;
    while (consumed < ep->txParse.size()) {
        if (ep->txParse[consumed] != 0xcc) {
            ++consumed;
            continue;
        }

        string          channel;
        vector<uint8_t> payload;
        size_t          frame_end = 0;
        if (!decode_frame(ep->txParse, consumed, frame_end, channel, payload)) break;

        bool drop = false;
        if (ep->dropEnabled && !ep->dropDone && channel == ep->dropChannel &&
            payload.size() >= PACKETIZED_HEADER_BYTES + PACKETIZED_DATA_OVERHEAD_BYTES) {
            uint8_t  type     = payload[0];
            uint8_t  body_len = payload[3];
            if (type == PACKETIZED_MSG_DATA &&
                payload.size() == (size_t)PACKETIZED_HEADER_BYTES + body_len &&
                body_len >= PACKETIZED_DATA_OVERHEAD_BYTES) {
                uint16_t packet_id = zcm_read_u16_be(
                    &payload[PACKETIZED_HEADER_BYTES]);
                if (packet_id == ep->dropPacketId) {
                    drop         = true;
                    ep->dropDone = true;
                }
            }
        }

        if (!drop) {
            ep->peer->rx.insert(ep->peer->rx.end(),
                                ep->txParse.begin() + (ptrdiff_t)consumed,
                                ep->txParse.begin() + (ptrdiff_t)frame_end);
        }

        consumed = frame_end;
    }

    if (consumed > 0) {
        ep->txParse.erase(ep->txParse.begin(), ep->txParse.begin() + (ptrdiff_t)consumed);
    }

    return nData;
}

static uint64_t fake_now(void* usr) { return *(uint64_t*)usr; }

static void pump(zcm_trans_t* tx, zcm_trans_t* rx, int iters)
{
    for (int i = 0; i < iters; ++i) {
        TS_ASSERT_EQUALS(packetized_serial_update_tx(tx), ZCM_EOK);
        TS_ASSERT_EQUALS(packetized_serial_update_rx(rx), ZCM_EOK);
    }
}

// pump via zcm_trans_update (the vtable path used by TransportSerial/TransportCan wrappers)
static void pumpViaUpdate(zcm_trans_t* tx, zcm_trans_t* rx, int iters)
{
    for (int i = 0; i < iters; ++i) {
        TS_ASSERT_EQUALS(zcm_trans_update(tx), ZCM_EOK);
        TS_ASSERT_EQUALS(zcm_trans_update(rx), ZCM_EOK);
    }
}

class PacketizedSerialTransportTest : public CxxTest::TestSuite
{
  public:
    void testPacketizedRoundTrip()
    {
        PacketizedLinkEndpoint a;
        PacketizedLinkEndpoint b;
        a.peer = &b;
        b.peer = &a;

        uint64_t     now = 1000;
        zcm_trans_t* tx  = zcm_trans_packetized_serial_create(
             endpoint_get, endpoint_put, &a, fake_now, &now, 64, 32768, 0, 1024);
        zcm_trans_t* rx = zcm_trans_packetized_serial_create(
            endpoint_get, endpoint_put, &b, fake_now, &now, 64, 32768, 0, 1024);
        TSM_ASSERT("failed creating transports", tx && rx);

        vector<uint8_t> payload(512);
        for (size_t i = 0; i < payload.size(); ++i) payload[i] = (uint8_t)(i ^ 0x5a);

        zcm_msg_t msg;
        msg.utime   = now;
        msg.channel = (char*)"$BIG";
        msg.len     = payload.size();
        msg.buf     = payload.data();

        TS_ASSERT_EQUALS(zcm_trans_sendmsg(tx, msg), ZCM_EOK);
        pump(tx, rx, 20);

        zcm_msg_t out;
        TS_ASSERT_EQUALS(zcm_trans_recvmsg(rx, &out, 0), ZCM_EOK);
        TS_ASSERT_EQUALS(string(out.channel), string("BIG"));
        TS_ASSERT_EQUALS(out.len, payload.size());
        TS_ASSERT_SAME_DATA(out.buf, payload.data(), payload.size());

        zcm_trans_destroy(tx);
        zcm_trans_destroy(rx);
    }

    void testRetransmissionRequestPath()
    {
        PacketizedLinkEndpoint a;
        PacketizedLinkEndpoint b;
        a.peer = &b;
        b.peer = &a;

        a.dropEnabled  = true;
        a.dropPacketId = 1;
        a.dropChannel  = "$RETX";

        uint64_t     now = 2000;
        zcm_trans_t* ta  = zcm_trans_packetized_serial_create(
             endpoint_get, endpoint_put, &a, fake_now, &now, 64, 32768, 0, 1024);
        zcm_trans_t* tb = zcm_trans_packetized_serial_create(
            endpoint_get, endpoint_put, &b, fake_now, &now, 64, 32768, 0, 1024);
        TSM_ASSERT("failed creating transports", ta && tb);

        vector<uint8_t> payload(700);
        for (size_t i = 0; i < payload.size(); ++i) payload[i] = (uint8_t)(i + 11);

        zcm_msg_t m;
        m.utime   = now;
        m.channel = (char*)"$RETX";
        m.len     = payload.size();
        m.buf     = payload.data();

        TS_ASSERT_EQUALS(zcm_trans_sendmsg(ta, m), ZCM_EOK);
        pump(ta, tb, 25);

        zcm_msg_t out;
        TS_ASSERT_EQUALS(zcm_trans_recvmsg(tb, &out, 0), ZCM_EAGAIN);

        now += 300000;
        TS_ASSERT_EQUALS(zcm_trans_recvmsg(tb, &out, 0), ZCM_EAGAIN);

        pump(tb, ta, 10);
        pump(ta, tb, 20);

        bool gotRetx = false;
        for (int i = 0; i < 6; ++i) {
            int ret = zcm_trans_recvmsg(tb, &out, 0);
            if (ret != ZCM_EOK) continue;
            if (string(out.channel) == "RETX") {
                gotRetx = true;
                TS_ASSERT_EQUALS(out.len, payload.size());
                TS_ASSERT_SAME_DATA(out.buf, payload.data(), payload.size());
                break;
            }
        }
        TS_ASSERT(gotRetx);
        TS_ASSERT(a.dropDone);

        zcm_trans_destroy(ta);
        zcm_trans_destroy(tb);
    }

    void testConfiguredMaxMessageSize()
    {
        PacketizedLinkEndpoint a;
        PacketizedLinkEndpoint b;
        a.peer = &b;
        b.peer = &a;

        uint64_t     now = 3000;
        zcm_trans_t* tx  = zcm_trans_packetized_serial_create(
             endpoint_get, endpoint_put, &a, fake_now, &now, 64, 32768, 0, 128);
        zcm_trans_t* rx = zcm_trans_packetized_serial_create(
            endpoint_get, endpoint_put, &b, fake_now, &now, 64, 32768, 0, 128);
        TSM_ASSERT("failed creating transports", tx && rx);

        vector<uint8_t> smallPayload(128, 0x5a);
        zcm_msg_t small;
        small.utime   = now;
        small.channel = (char*)"$SMALL";
        small.len     = smallPayload.size();
        small.buf     = smallPayload.data();
        TS_ASSERT_EQUALS(zcm_trans_sendmsg(tx, small), ZCM_EOK);

        vector<uint8_t> largePayload(129, 0x6b);
        zcm_msg_t large;
        large.utime   = now;
        large.channel = (char*)"$LARGE";
        large.len     = largePayload.size();
        large.buf     = largePayload.data();
        TS_ASSERT_EQUALS(zcm_trans_sendmsg(tx, large), ZCM_EINVALID);

        zcm_trans_destroy(tx);
        zcm_trans_destroy(rx);
    }

    void testAsymmetricPacketSizesRoundTrip()
    {
        PacketizedLinkEndpoint a;
        PacketizedLinkEndpoint b;
        a.peer = &b;
        b.peer = &a;

        uint64_t     now = 4000;
        zcm_trans_t* tx  = zcm_trans_packetized_serial_create(
             endpoint_get, endpoint_put, &a, fake_now, &now, 64, 32768, 8, 1024);
        zcm_trans_t* rx = zcm_trans_packetized_serial_create(
            endpoint_get, endpoint_put, &b, fake_now, &now, 64, 32768, 32, 1024);
        TSM_ASSERT("failed creating transports", tx && rx);

        vector<uint8_t> payload(512);
        for (size_t i = 0; i < payload.size(); ++i) payload[i] = (uint8_t)(i ^ 0x33);

        zcm_msg_t msg;
        msg.utime   = now;
        msg.channel = (char*)"$ASYM";
        msg.len     = payload.size();
        msg.buf     = payload.data();

        TS_ASSERT_EQUALS(zcm_trans_sendmsg(tx, msg), ZCM_EOK);
        pump(tx, rx, 40);

        zcm_msg_t out;
        TS_ASSERT_EQUALS(zcm_trans_recvmsg(rx, &out, 0), ZCM_EOK);
        TS_ASSERT_EQUALS(string(out.channel), string("ASYM"));
        TS_ASSERT_EQUALS(out.len, payload.size());
        TS_ASSERT_SAME_DATA(out.buf, payload.data(), payload.size());

        zcm_trans_destroy(tx);
        zcm_trans_destroy(rx);
    }

    void testAsymmetricPacketSizesReverseRoundTrip()
    {
        PacketizedLinkEndpoint a;
        PacketizedLinkEndpoint b;
        a.peer = &b;
        b.peer = &a;

        uint64_t     now = 5000;
        zcm_trans_t* tx  = zcm_trans_packetized_serial_create(
             endpoint_get, endpoint_put, &a, fake_now, &now, 64, 32768, 32, 1024);
        zcm_trans_t* rx = zcm_trans_packetized_serial_create(
            endpoint_get, endpoint_put, &b, fake_now, &now, 64, 32768, 8, 1024);
        TSM_ASSERT("failed creating transports", tx && rx);

        vector<uint8_t> payload(512);
        for (size_t i = 0; i < payload.size(); ++i) payload[i] = (uint8_t)(i ^ 0x77);

        zcm_msg_t msg;
        msg.utime   = now;
        msg.channel = (char*)"$ASYM_REV";
        msg.len     = payload.size();
        msg.buf     = payload.data();

        TS_ASSERT_EQUALS(zcm_trans_sendmsg(tx, msg), ZCM_EOK);
        pump(tx, rx, 40);

        zcm_msg_t out;
        TS_ASSERT_EQUALS(zcm_trans_recvmsg(rx, &out, 0), ZCM_EOK);
        TS_ASSERT_EQUALS(string(out.channel), string("ASYM_REV"));
        TS_ASSERT_EQUALS(out.len, payload.size());
        TS_ASSERT_SAME_DATA(out.buf, payload.data(), payload.size());

        zcm_trans_destroy(tx);
        zcm_trans_destroy(rx);
    }

    void testNonPacketizedPassthroughIgnoresPacketizedSizeLimit()
    {
        PacketizedLinkEndpoint a;
        PacketizedLinkEndpoint b;
        a.peer = &b;
        b.peer = &a;

        uint64_t     now = 5500;
        zcm_trans_t* tx  = zcm_trans_packetized_serial_create(
             endpoint_get, endpoint_put, &a, fake_now, &now, 2048, 32768, 8, 128);
        zcm_trans_t* rx = zcm_trans_packetized_serial_create(
            endpoint_get, endpoint_put, &b, fake_now, &now, 2048, 32768, 8, 128);
        TSM_ASSERT("failed creating transports", tx && rx);

        vector<uint8_t> payload(512);
        for (size_t i = 0; i < payload.size(); ++i) payload[i] = (uint8_t)(i ^ 0x19);

        zcm_msg_t msg;
        msg.utime   = now;
        msg.channel = (char*)"PLAIN";
        msg.len     = payload.size();
        msg.buf     = payload.data();

        TS_ASSERT_EQUALS(zcm_trans_sendmsg(tx, msg), ZCM_EOK);
        pump(tx, rx, 20);

        zcm_msg_t out;
        TS_ASSERT_EQUALS(zcm_trans_recvmsg(rx, &out, 0), ZCM_EOK);
        TS_ASSERT_EQUALS(string(out.channel), string("PLAIN"));
        TS_ASSERT_EQUALS(out.len, payload.size());
        TS_ASSERT_SAME_DATA(out.buf, payload.data(), payload.size());

        zcm_trans_destroy(tx);
        zcm_trans_destroy(rx);
    }

    void testPacketizedMessageStillRespectsConfiguredSizeLimit()
    {
        PacketizedLinkEndpoint a;
        PacketizedLinkEndpoint b;
        a.peer = &b;
        b.peer = &a;

        uint64_t     now = 5600;
        zcm_trans_t* tx  = zcm_trans_packetized_serial_create(
             endpoint_get, endpoint_put, &a, fake_now, &now, 2048, 32768, 8, 128);
        zcm_trans_t* rx = zcm_trans_packetized_serial_create(
            endpoint_get, endpoint_put, &b, fake_now, &now, 2048, 32768, 8, 128);
        TSM_ASSERT("failed creating transports", tx && rx);

        vector<uint8_t> payload(129, 0x2a);
        zcm_msg_t msg;
        msg.utime   = now;
        msg.channel = (char*)"$TOO_BIG";
        msg.len     = payload.size();
        msg.buf     = payload.data();

        TS_ASSERT_EQUALS(zcm_trans_sendmsg(tx, msg), ZCM_EINVALID);

        zcm_trans_destroy(tx);
        zcm_trans_destroy(rx);
    }

    // Passing max_message_size=0 enables dynamic mode. Buffers start at
    // PACKETIZED_DEFAULT_MAX_MESSAGE_SIZE (1024) and grow to fit any message.
    void testDynamicResizingRoundTrip()
    {
        PacketizedLinkEndpoint a;
        PacketizedLinkEndpoint b;
        a.peer = &b;
        b.peer = &a;

        uint64_t     now = 7000;
        zcm_trans_t* tx  = zcm_trans_packetized_serial_create(
             endpoint_get, endpoint_put, &a, fake_now, &now, 64, 32768, 0, 0);
        zcm_trans_t* rx = zcm_trans_packetized_serial_create(
            endpoint_get, endpoint_put, &b, fake_now, &now, 64, 32768, 0, 0);
        TSM_ASSERT("failed creating transports", tx && rx);

        // 2048 bytes exceeds PACKETIZED_DEFAULT_MAX_MESSAGE_SIZE (1024)
        vector<uint8_t> payload(2048);
        for (size_t i = 0; i < payload.size(); ++i) payload[i] = (uint8_t)(i ^ 0xd5);

        zcm_msg_t msg;
        msg.utime   = now;
        msg.channel = (char*)"$DYN";
        msg.len     = payload.size();
        msg.buf     = payload.data();

        TS_ASSERT_EQUALS(zcm_trans_sendmsg(tx, msg), ZCM_EOK);
        pump(tx, rx, 40);

        zcm_msg_t out;
        TS_ASSERT_EQUALS(zcm_trans_recvmsg(rx, &out, 0), ZCM_EOK);
        TS_ASSERT_EQUALS(string(out.channel), string("DYN"));
        TS_ASSERT_EQUALS(out.len, payload.size());
        TS_ASSERT_SAME_DATA(out.buf, payload.data(), payload.size());

        zcm_trans_destroy(tx);
        zcm_trans_destroy(rx);
    }

    void testDynamicResizingMultipleGrowths()
    {
        PacketizedLinkEndpoint a;
        PacketizedLinkEndpoint b;
        a.peer = &b;
        b.peer = &a;

        uint64_t     now = 7100;
        zcm_trans_t* tx  = zcm_trans_packetized_serial_create(
             endpoint_get, endpoint_put, &a, fake_now, &now, 64, 32768, 0, 0);
        zcm_trans_t* rx = zcm_trans_packetized_serial_create(
            endpoint_get, endpoint_put, &b, fake_now, &now, 64, 32768, 0, 0);
        TSM_ASSERT("failed creating transports", tx && rx);

        static const size_t sizes[] = { 512, 2048, 8192 };
        for (size_t si = 0; si < 3; ++si) {
            vector<uint8_t> payload(sizes[si]);
            for (size_t i = 0; i < payload.size(); ++i) payload[i] = (uint8_t)(i ^ (uint8_t)si);

            zcm_msg_t msg;
            msg.utime   = now;
            msg.channel = (char*)"$GROW";
            msg.len     = payload.size();
            msg.buf     = payload.data();

            TS_ASSERT_EQUALS(zcm_trans_sendmsg(tx, msg), ZCM_EOK);
            pump(tx, rx, 200);

            zcm_msg_t out;
            TS_ASSERT_EQUALS(zcm_trans_recvmsg(rx, &out, 0), ZCM_EOK);
            TS_ASSERT_EQUALS(string(out.channel), string("GROW"));
            TS_ASSERT_EQUALS(out.len, payload.size());
            TS_ASSERT_SAME_DATA(out.buf, payload.data(), payload.size());
        }

        zcm_trans_destroy(tx);
        zcm_trans_destroy(rx);
    }

    // Drive the transport via zcm_trans_update (the vtable update path) rather than the
    // explicit packetized_serial_update_rx/tx functions. This mirrors how the
    // TransportSerial and TransportCan wrappers operate and would have caught the
    // assertion failure caused by calling serial_update_rx/tx on a packetized transport.
    void testRoundTripViaVtableUpdate()
    {
        PacketizedLinkEndpoint a;
        PacketizedLinkEndpoint b;
        a.peer = &b;
        b.peer = &a;

        uint64_t     now = 6000;
        zcm_trans_t* tx  = zcm_trans_packetized_serial_create(
             endpoint_get, endpoint_put, &a, fake_now, &now, 64, 32768, 8, 1024);
        zcm_trans_t* rx = zcm_trans_packetized_serial_create(
            endpoint_get, endpoint_put, &b, fake_now, &now, 64, 32768, 8, 1024);
        TSM_ASSERT("failed creating transports", tx && rx);

        vector<uint8_t> payload(256);
        for (size_t i = 0; i < payload.size(); ++i) payload[i] = (uint8_t)(i ^ 0xab);

        zcm_msg_t msg;
        msg.utime   = now;
        msg.channel = (char*)"$VTABLE";
        msg.len     = payload.size();
        msg.buf     = payload.data();

        TS_ASSERT_EQUALS(zcm_trans_sendmsg(tx, msg), ZCM_EOK);
        pumpViaUpdate(tx, rx, 40);

        zcm_msg_t out;
        TS_ASSERT_EQUALS(zcm_trans_recvmsg(rx, &out, 0), ZCM_EOK);
        TS_ASSERT_EQUALS(string(out.channel), string("VTABLE"));
        TS_ASSERT_EQUALS(out.len, payload.size());
        TS_ASSERT_SAME_DATA(out.buf, payload.data(), payload.size());

        zcm_trans_destroy(tx);
        zcm_trans_destroy(rx);
    }
};

#endif
