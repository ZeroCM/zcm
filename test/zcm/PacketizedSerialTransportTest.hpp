#ifndef PACKETIZED_SERIAL_TRANSPORT_TEST_HPP
#define PACKETIZED_SERIAL_TRANSPORT_TEST_HPP

#include "cxxtest/TestSuite.h"

extern "C" {
#include "zcm/transport.h"
zcm_trans_t* zcm_trans_packetized_serial_create(
    size_t (*get)(uint8_t* data, size_t nData, void* usr),
    size_t (*put)(const uint8_t* data, size_t nData, void* usr), void* put_get_usr,
    uint64_t (*timestamp_now)(void* usr), void* time_usr, size_t MTU, size_t bufSize);
int packetized_serial_update_rx(zcm_trans_t* zt);
int packetized_serial_update_tx(zcm_trans_t* zt);
}

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
            payload.size() >= 6) {
            uint8_t type     = payload[0];
            uint8_t body_len = payload[3];
            if (type == 2 && payload.size() == 4u + body_len && body_len >= 2) {
                uint16_t packet_id = ((uint16_t)payload[4] << 8) | (uint16_t)payload[5];
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
            endpoint_get, endpoint_put, &a, fake_now, &now, 64, 32768);
        zcm_trans_t* rx = zcm_trans_packetized_serial_create(
            endpoint_get, endpoint_put, &b, fake_now, &now, 64, 32768);
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
        TS_ASSERT_EQUALS(string(out.channel), string("$BIG"));
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
            endpoint_get, endpoint_put, &a, fake_now, &now, 64, 32768);
        zcm_trans_t* tb = zcm_trans_packetized_serial_create(
            endpoint_get, endpoint_put, &b, fake_now, &now, 64, 32768);
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

        uint8_t   pingbuf[1] = { 0x42 };
        zcm_msg_t ping;
        ping.utime   = now;
        ping.channel = (char*)"PING";
        ping.len     = 1;
        ping.buf     = pingbuf;

        TS_ASSERT_EQUALS(zcm_trans_sendmsg(tb, ping), ZCM_EOK);
        pump(tb, ta, 10);

        TS_ASSERT_EQUALS(zcm_trans_recvmsg(ta, &out, 0), ZCM_EOK);

        uint8_t   pongbuf[1] = { 0x24 };
        zcm_msg_t pong;
        pong.utime   = now;
        pong.channel = (char*)"PONG";
        pong.len     = 1;
        pong.buf     = pongbuf;

        TS_ASSERT_EQUALS(zcm_trans_sendmsg(ta, pong), ZCM_EOK);
        pump(ta, tb, 20);

        bool gotRetx = false;
        for (int i = 0; i < 6; ++i) {
            int ret = zcm_trans_recvmsg(tb, &out, 0);
            if (ret != ZCM_EOK) continue;
            if (string(out.channel) == "$RETX") {
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
};

#endif
