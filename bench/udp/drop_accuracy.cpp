// Checks that the UDP transport's drop counter (zcm_query_drops) matches the
// real number of lost messages.
//
// A sender blasts a mix of short and fragmented messages over loopback UDP at
// a slow receiver so the kernel socket buffer overflows. Every message the
// sender attempts consumes one sequence number, so ground truth is simply
// attempted - received. A few spaced-out "fence" messages at the end make sure
// losses at the tail are detected too.
//
// Usage: drop_accuracy [num_messages] [big_message_bytes]

#include <atomic>
#include <chrono>
#include <cinttypes>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <thread>
#include <vector>

#include <unistd.h>

#include <zcm/zcm.h>
#include <zcm/url.h>
#include <zcm/transport.h>
#include <zcm/transport_registrar.h>

static zcm_trans_t *makeTrans(const std::string& url)
{
    zcm_url_t *u = zcm_url_create(url.c_str());
    zcm_trans_create_func *creator = zcm_transport_find(zcm_url_protocol(u));
    zcm_trans_t *t = creator ? creator(u, NULL) : NULL;
    zcm_url_destroy(u);
    return t;
}

int main(int argc, char *argv[])
{
    size_t numMsgs = argc > 1 ? strtoul(argv[1], NULL, 10) : 20000;
    size_t bigSize = argc > 2 ? strtoul(argv[2], NULL, 10) : 40000;
    const size_t numFences = 5;

    unsigned port = 40000 + getpid() % 10000;
    std::string a = std::to_string(port), b = std::to_string(port + 1);
    zcm_trans_t *rx = makeTrans("udp://127.0.0.1:" + a + ":" + b);
    zcm_trans_t *tx = makeTrans("udp://127.0.0.1:" + b + ":" + a);
    if (!rx || !tx) {
        fprintf(stderr, "failed to create udp transports\n");
        return 1;
    }

    std::atomic<bool> senderDone {false};
    std::atomic<uint64_t> received {0};

    std::thread recvThread([&]() {
        auto lastMsg = std::chrono::steady_clock::now();
        while (true) {
            zcm_msg_t msg;
            if (zcm_trans_recvmsg(rx, &msg, 50) == ZCM_EOK) {
                // A slow consumer: stall now and then so the socket overflows
                if (++received % 500 == 0) usleep(20000);
                lastMsg = std::chrono::steady_clock::now();
            } else if (senderDone &&
                       std::chrono::steady_clock::now() - lastMsg > std::chrono::seconds(1)) {
                break;
            }
        }
    });

    std::vector<uint8_t> small(200, 0x11), big(bigSize, 0x22);
    uint64_t attempted = 0;
    for (size_t i = 0; i < numMsgs; i++) {
        auto& buf = (i % 4 == 0) ? big : small;
        zcm_msg_t msg;
        msg.utime = 0;
        msg.channel = "BENCH";
        msg.len = buf.size();
        msg.buf = buf.data();
        zcm_trans_sendmsg(tx, msg);
        attempted++;
    }
    for (size_t i = 0; i < numFences; i++) {
        usleep(200000);
        zcm_msg_t msg;
        msg.utime = 0;
        msg.channel = "BENCH";
        msg.len = small.size();
        msg.buf = small.data();
        zcm_trans_sendmsg(tx, msg);
        attempted++;
    }
    senderDone = true;
    recvThread.join();

    uint64_t drops = 0;
    if (zcm_trans_query_drops(rx, &drops) != ZCM_EOK) {
        fprintf(stderr, "query_drops not supported\n");
        return 1;
    }

    uint64_t lost = attempted - received;
    printf("attempted %" PRIu64 ", received %" PRIu64 ", lost %" PRIu64
           ", reported drops %" PRIu64 " -> %s\n",
           attempted, received.load(), lost, drops, lost == drops ? "MATCH" : "MISMATCH");

    zcm_trans_destroy(tx);
    zcm_trans_destroy(rx);
    return lost == drops ? 0 : 2;
}
