#ifndef UDPDROPSTEST_HPP
#define UDPDROPSTEST_HPP

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <atomic>
#include <cstring>
#include <string>
#include <thread>
#include <vector>

#include "cxxtest/TestSuite.h"
#include "zcm/zcm.h"
#include "zcm/transport.h"
#include "zcm/transport_registrar.h"

// Exercises the UDP transport's drop accounting (zcm_query_drops) by sending
// hand-built ZCM UDP packets from raw sockets over loopback, so every test can
// decide exactly which packets "go missing".

namespace udpdrops {

static const uint32_t MAGIC_SHORT = 0x4c433032;
static const uint32_t MAGIC_LONG  = 0x4c433033;
static const char    *kChannel    = "UDP_DROPS";

static uint16_t nextPort()
{
    static uint16_t port = 0;
    if (port == 0) port = 20000 + (uint16_t)(getpid() % 20000);
    port += 2;
    return port;
}

// A raw UDP socket that pretends to be a ZCM publisher
struct RawSender
{
    int fd = -1;
    struct sockaddr_in dest;

    explicit RawSender(uint16_t destPort)
    {
        fd = socket(AF_INET, SOCK_DGRAM, 0);
        memset(&dest, 0, sizeof(dest));
        dest.sin_family = AF_INET;
        dest.sin_port = htons(destPort);
        inet_aton("127.0.0.1", &dest.sin_addr);
    }
    ~RawSender() { if (fd >= 0) close(fd); }

    void sendRaw(const std::vector<uint8_t>& pkt)
    {
        ssize_t n = sendto(fd, pkt.data(), pkt.size(), 0,
                           (struct sockaddr*)&dest, sizeof(dest));
        TS_ASSERT_EQUALS(n, (ssize_t)pkt.size());
    }

    static void put32(std::vector<uint8_t>& v, uint32_t x)
    {
        x = htonl(x);
        const uint8_t *p = (const uint8_t*)&x;
        v.insert(v.end(), p, p + 4);
    }

    static void put16(std::vector<uint8_t>& v, uint16_t x)
    {
        x = htons(x);
        const uint8_t *p = (const uint8_t*)&x;
        v.insert(v.end(), p, p + 2);
    }

    void sendShort(uint32_t seqno, const std::string& payload = "x")
    {
        std::vector<uint8_t> v;
        put32(v, MAGIC_SHORT);
        put32(v, seqno);
        v.insert(v.end(), kChannel, kChannel + strlen(kChannel) + 1);
        v.insert(v.end(), payload.begin(), payload.end());
        sendRaw(v);
    }

    // Fragment 'fragNo' of a message with 'nfrags' fragments carrying
    // FRAG_DATA bytes of data each. Fragment 0 also carries the channel.
    static const uint32_t FRAG_DATA = 10;
    void sendFragment(uint32_t seqno, uint16_t fragNo, uint16_t nfrags,
                      uint32_t offsetOverride = UINT32_MAX)
    {
        uint32_t msgSize = FRAG_DATA * nfrags;
        uint32_t offset = offsetOverride != UINT32_MAX ? offsetOverride : FRAG_DATA * fragNo;
        std::vector<uint8_t> v;
        put32(v, MAGIC_LONG);
        put32(v, seqno);
        put32(v, msgSize);
        put32(v, offset);
        put16(v, fragNo);
        put16(v, nfrags);
        if (fragNo == 0) v.insert(v.end(), kChannel, kChannel + strlen(kChannel) + 1);
        for (uint32_t i = 0; i < FRAG_DATA; i++) v.push_back((uint8_t)(seqno + i));
        sendRaw(v);
    }

    void sendFragmented(uint32_t seqno, uint16_t nfrags)
    {
        for (uint16_t f = 0; f < nfrags; f++) sendFragment(seqno, f, nfrags);
    }
};

// The receiving side: a real ZCM UDP transport
struct Receiver
{
    zcm_trans_t *trans = nullptr;
    uint16_t port;

    Receiver() : port(nextPort())
    {
        std::string url = "udp://127.0.0.1:" + std::to_string(port) + ":" +
                          std::to_string(port + 1);
        zcm_url_t *u = zcm_url_create(url.c_str());
        zcm_trans_create_func *creator = zcm_transport_find(zcm_url_protocol(u));
        TS_ASSERT(creator);
        if (creator) trans = creator(u, NULL);
        zcm_url_destroy(u);
        TS_ASSERT(trans);
    }
    ~Receiver() { if (trans) zcm_trans_destroy(trans); }

    // Receive until the socket has been quiet for a while
    size_t drain()
    {
        size_t n = 0;
        if (!trans) return n;
        zcm_msg_t msg;
        while (zcm_trans_recvmsg(trans, &msg, 100) == ZCM_EOK) {
            TS_ASSERT_EQUALS(std::string(msg.channel), std::string(kChannel));
            n++;
        }
        return n;
    }

    uint64_t drops()
    {
        uint64_t d = 12345;
        if (!trans) return d;
        TS_ASSERT_EQUALS(zcm_trans_query_drops(trans, &d), ZCM_EOK);
        return d;
    }
};

} // namespace udpdrops

using namespace udpdrops;

class UdpDropsTest : public CxxTest::TestSuite
{
  public:
    void testQueryDropsIsImplemented()
    {
        Receiver rx;
        TS_ASSERT_EQUALS(rx.drops(), 0u);
        if (rx.trans)
            TS_ASSERT_EQUALS(zcm_trans_query_drops(rx.trans, NULL), ZCM_EINVALID);
    }

    void testNoDropsForInOrderTraffic()
    {
        Receiver rx;
        RawSender tx(rx.port);
        for (uint32_t i = 0; i < 20; i++) tx.sendShort(i);
        for (uint32_t i = 20; i < 25; i++) tx.sendFragmented(i, 3);
        TS_ASSERT_EQUALS(rx.drain(), 25u);
        TS_ASSERT_EQUALS(rx.drops(), 0u);
    }

    void testSequenceGapCountsMissingMessages()
    {
        Receiver rx;
        RawSender tx(rx.port);
        tx.sendShort(0);
        tx.sendShort(1);
        tx.sendShort(5);    // 2, 3, 4 never arrive
        tx.sendShort(6);
        tx.sendFragmented(9, 2); // 7, 8 never arrive
        TS_ASSERT_EQUALS(rx.drain(), 5u);
        TS_ASSERT_EQUALS(rx.drops(), 5u);
    }

    void testMissingFragmentDropsOneMessage()
    {
        Receiver rx;
        RawSender tx(rx.port);
        tx.sendFragment(0, 0, 3);
        tx.sendFragment(0, 2, 3); // fragment 1 is lost
        tx.sendShort(1);
        tx.sendFragmented(2, 3);  // replaces the stale partial message
        TS_ASSERT_EQUALS(rx.drain(), 2u);
        TS_ASSERT_EQUALS(rx.drops(), 1u);
    }

    void testPartialMessageFollowedOnlyByShortMessages()
    {
        Receiver rx;
        RawSender tx(rx.port);
        tx.sendFragment(0, 0, 3); // fragments 1 and 2 are lost
        tx.sendShort(1);
        tx.sendShort(2);
        TS_ASSERT_EQUALS(rx.drain(), 2u);
        TS_ASSERT_EQUALS(rx.drops(), 1u);
        tx.sendFragment(0, 2, 3); // a straggler doesn't count it twice
        tx.sendShort(3);
        TS_ASSERT_EQUALS(rx.drain(), 1u);
        TS_ASSERT_EQUALS(rx.drops(), 1u);
    }

    void testFragmentsWithoutFirstCountOnce()
    {
        Receiver rx;
        RawSender tx(rx.port);
        tx.sendShort(0);
        tx.sendFragment(1, 1, 4); // fragment 0 is lost
        tx.sendFragment(1, 2, 4);
        tx.sendFragment(1, 3, 4);
        tx.sendShort(2);
        TS_ASSERT_EQUALS(rx.drain(), 2u);
        TS_ASSERT_EQUALS(rx.drops(), 1u);
    }

    void testInvalidFragmentCountsOnce()
    {
        Receiver rx;
        RawSender tx(rx.port);
        tx.sendFragment(0, 0, 3);
        tx.sendFragment(0, 1, 3, 1000000); // offset past the end of the message
        tx.sendFragment(0, 2, 3);          // now an orphan of the same message
        tx.sendShort(1);
        TS_ASSERT_EQUALS(rx.drain(), 1u);
        TS_ASSERT_EQUALS(rx.drops(), 1u);
    }

    void testGapAndPartialMessageNotDoubleCounted()
    {
        Receiver rx;
        RawSender tx(rx.port);
        tx.sendShort(0);
        tx.sendShort(3);           // 1 and 2 counted as missing
        tx.sendFragment(2, 1, 2);  // a late fragment of 2 must not count it again
        tx.sendShort(4);
        TS_ASSERT_EQUALS(rx.drain(), 3u);
        TS_ASSERT_EQUALS(rx.drops(), 2u);
    }

    void testDuplicateFragmentAfterCompletionIsNotADrop()
    {
        Receiver rx;
        RawSender tx(rx.port);
        tx.sendFragmented(0, 2);
        tx.sendFragment(0, 1, 2); // network duplicate of the last fragment
        tx.sendShort(1);
        TS_ASSERT_EQUALS(rx.drain(), 2u);
        TS_ASSERT_EQUALS(rx.drops(), 0u);
    }

    void testReorderedPacketStaysCounted()
    {
        // A message that shows up after a newer one was already counted as
        // missing; the counter never goes backwards.
        Receiver rx;
        RawSender tx(rx.port);
        tx.sendShort(0);
        tx.sendShort(2);
        TS_ASSERT_EQUALS(rx.drain(), 2u);
        TS_ASSERT_EQUALS(rx.drops(), 1u);
        tx.sendShort(1);
        tx.sendShort(3);
        TS_ASSERT_EQUALS(rx.drain(), 2u);
        TS_ASSERT_EQUALS(rx.drops(), 1u);
    }

    void testSequenceNumberWraparound()
    {
        Receiver rx;
        RawSender tx(rx.port);
        tx.sendShort(0xfffffffe);
        tx.sendShort(0xffffffff);
        tx.sendShort(0);
        tx.sendShort(2); // 1 is lost
        TS_ASSERT_EQUALS(rx.drain(), 4u);
        TS_ASSERT_EQUALS(rx.drops(), 1u);
    }

    void testSenderRestartIsNotADrop()
    {
        Receiver rx;
        RawSender tx(rx.port);
        for (uint32_t i = 50000; i < 50005; i++) tx.sendShort(i);
        for (uint32_t i = 0; i < 5; i++) tx.sendShort(i); // counter reset
        TS_ASSERT_EQUALS(rx.drain(), 10u);
        TS_ASSERT_EQUALS(rx.drops(), 0u);
    }

    void testSendersAreTrackedIndependently()
    {
        Receiver rx;
        RawSender a(rx.port), b(rx.port);
        for (uint32_t i = 0; i < 10; i++) {
            a.sendShort(i);
            if (i != 4) b.sendShort(100 + i); // b loses one message
        }
        TS_ASSERT_EQUALS(rx.drain(), 19u);
        TS_ASSERT_EQUALS(rx.drops(), 1u);
    }

    void testLargeGapIsCountedInFull()
    {
        Receiver rx;
        RawSender tx(rx.port);
        tx.sendShort(0);
        tx.sendShort(1001); // more than the 64 message dedupe window
        tx.sendShort(1002);
        TS_ASSERT_EQUALS(rx.drain(), 3u);
        TS_ASSERT_EQUALS(rx.drops(), 1000u);
    }

    void testZcmQueryDropsEndToEnd()
    {
        // Real ZCM publisher and subscriber over UDP unicast, including
        // messages large enough to be fragmented: nothing should be dropped.
        uint16_t p = nextPort();
        std::string subUrl = "udp://127.0.0.1:" + std::to_string(p) + ":" + std::to_string(p + 1);
        std::string pubUrl = "udp://127.0.0.1:" + std::to_string(p + 1) + ":" + std::to_string(p);
        zcm_t *sub = zcm_create(subUrl.c_str());
        zcm_t *pub = zcm_create(pubUrl.c_str());
        TS_ASSERT(sub && pub);
        if (!sub || !pub) return;

        static std::atomic<int> received;
        received = 0;
        auto handler = [](const zcm_recv_buf_t *rbuf, const char *channel, void *usr) {
            received++;
        };
        zcm_subscribe(sub, kChannel, handler, NULL);
        zcm_start(sub);

        const int kNumMsgs = 50;
        std::vector<uint8_t> small(100, 0xab), big(20000, 0xcd);
        for (int i = 0; i < kNumMsgs; i++) {
            auto& buf = (i % 2) ? big : small;
            zcm_publish(pub, kChannel, buf.data(), buf.size());
            zcm_flush(pub);
            usleep(1000);
        }
        for (int i = 0; i < 200 && received < kNumMsgs; i++) usleep(10000);

        uint64_t drops = 12345;
        TS_ASSERT_EQUALS(zcm_query_drops(sub, &drops), ZCM_EOK);
        TS_ASSERT_EQUALS(received.load(), kNumMsgs);
        TS_ASSERT_EQUALS(drops, 0u);

        zcm_stop(sub);
        zcm_destroy(sub);
        zcm_destroy(pub);
    }
};

#endif // UDPDROPSTEST_HPP
