#include "udp.hpp"
#include "buffers.hpp"
#include "udpsocket.hpp"
#include "mempool.hpp"
#include "losstracker.hpp"

#include "zcm/transport.h"
#include "zcm/transport_registrar.h"
#include "zcm/transport_register.hpp"

#include "util/StringUtil.hpp"

#define MTU (1<<28)

static i32 utimeInSeconds()
{
    struct timeval tv;
    gettimeofday (&tv, NULL);
    return (i32)tv.tv_sec;
}

/**
 * udp_params_t:
 * @addr:           unicast/multicast address
 * @sub_port:       unicast/multicast subscription port
 * @pub_port:       unicast/multicast publish port
 * @ttl:            if 0, then packets never leave local host.
 *                  if 1, then packets stay on the local network
 *                        and never traverse a router
 *                  don't use > 1.  that's just rude.
 * @recv_buf_size:  requested size of the kernel receive buffer, set with
 *                  SO_RCVBUF.  0 indicates to use the default settings.
 *
 */
struct Params
{
    string ip;
    struct in_addr addr;
    u16            sub_port;
    u16            pub_port;
    size_t         recv_buf_size;
    u8             ttl;
    bool           multicast;

    Params(const string& ip, u16 sub_port, u16 pub_port,
           size_t recv_buf_size, u8 ttl, bool multicast) :
        ip(ip), sub_port(sub_port), pub_port(pub_port),
        recv_buf_size(recv_buf_size), ttl(ttl), multicast(multicast)
    {
        // TODO verify that the IP and PORT are vaild
        inet_aton(ip.c_str(), (struct in_addr*) &this->addr);
    }
};

struct UDP
{
    Params params;
    UDPAddress destAddr;

    UDPSocket recvfd;
    UDPSocket sendfd;

    /* size of the kernel UDP receive buffer */
    size_t kernel_rbuf_sz = 0;
    size_t kernel_sbuf_sz = 0;
    bool warned_about_small_kernel_buf = false;

    MessagePool pool {MAX_FRAG_BUF_TOTAL_SIZE, MAX_NUM_FRAG_BUFS};

    /* other variables */
    u64          udp_rx = 0;            // messages received and delivered
    u64          udp_discarded_bad = 0; // packets discarded because they were bad somehow
    i32          udp_last_report_secs = 0;
    u64          udp_last_report_drops = 0;
    u64          udp_last_report_kernel_drops = 0;

    // Messages lost between senders and us, reported through query_drops()
    LossTracker  loss;

    u32          msg_seqno = 0; // rolling counter of how many messages transmitted

    /***** Methods ******/
    UDP(const string& ip, u16 sub_port, u16 pub_port,
        size_t recv_buf_size, u8 ttl, bool multicast);
    bool init();
    ~UDP();

    int handle();

    int sendmsg(zcm_msg_t msg);
    int recvmsg(zcm_msg_t *msg, unsigned timeoutMs);
    int queryDrops(uint64_t *out_drops);

  private:
    // These returns non-null when a full message has been received
    Message *recvShort(Packet *pkt, u32 sz);
    Message *recvFragment(Packet *pkt, u32 sz);
    Message *readMessage(unsigned timeoutMs);

    Message *m = nullptr;

    bool selftest();
    void checkForMessageLoss();
};

Message *UDP::recvShort(Packet *pkt, u32 sz)
{
    MsgHeaderShort *hdr = pkt->asHeaderShort();

    struct sockaddr_in *from = (struct sockaddr_in*)&pkt->from;
    u32 msg_seqno = hdr->getMsgSeqno();
    loss.observe(from, msg_seqno);

    // A sender transmits one message at a time, so a newer message means any
    // partially received fragmented message from it can no longer complete.
    // (recvFragment already gives up on it when the next fragmented message
    // starts; without this a run of short messages would keep it around.)
    FragBuf *fbuf = pool.lookupFragBuf(from);
    if (fbuf && (u32)(msg_seqno - fbuf->msg_seqno) - 1 < 0x7fffffffu) {
        ZCM_DEBUG("Dropping message (missing %d fragments)", fbuf->fragments_remaining);
        loss.markDropped(from, fbuf->msg_seqno);
        pool.removeFragBuf(fbuf);
    }

    size_t clen = hdr->getChannelLen();
    if (clen > ZCM_CHANNEL_MAXLEN) {
        ZCM_DEBUG("bad channel name length");
        udp_discarded_bad++;
        loss.markDropped(from, msg_seqno);
        return NULL;
    }

    udp_rx++;
    loss.markDelivered(from, msg_seqno);

    Message *msg = pool.allocMessageEmpty();
    msg->utime = pkt->utime;
    msg->channel = hdr->getChannelPtr();
    msg->channellen = clen;
    msg->data = hdr->getDataPtr();
    msg->datalen = hdr->getDataLen(sz);
    pool.moveBuffer(msg->buf, pkt->buf);

    return msg;
}

Message *UDP::recvFragment(Packet *pkt, u32 sz)
{
    MsgHeaderLong *hdr = pkt->asHeaderLong();

    struct sockaddr_in *from = (struct sockaddr_in*)&pkt->from;

    // any existing fragment buffer for this message source?
    FragBuf *fbuf = pool.lookupFragBuf(from);

    u32 msg_seqno = hdr->getMsgSeqno();
    u32 data_size = hdr->getMsgSize();
    u32 fragment_offset = hdr->getFragmentOffset();
    u16 fragment_no = hdr->getFragmentNo();
    u16 fragments_in_msg = hdr->getFragmentsInMsg();
    u32 frag_size = hdr->getFragmentSize(sz);
    char *data_start = hdr->getDataPtr();

    loss.observe(from, msg_seqno);

    // discard any stale fragments from previous messages
    if (fbuf && ((fbuf->msg_seqno != msg_seqno) ||
                 (fbuf->buf.size != data_size + fbuf->channellen+1))) {
        ZCM_DEBUG("Dropping message (missing %d fragments)", fbuf->fragments_remaining);
        loss.markDropped(from, fbuf->msg_seqno);
        pool.removeFragBuf(fbuf);
        fbuf = NULL;
    }

    if (data_size > MTU) {
        ZCM_DEBUG("rejecting huge message (%d bytes)", data_size);
        udp_discarded_bad++;
        loss.markDropped(from, msg_seqno);
        return NULL;
    }

    // create a new fragment buffer if necessary
    if (!fbuf && fragment_no == 0) {
        char *channel = (char*) (hdr + 1);
        int channel_sz = strlen(channel);
        if (channel_sz > ZCM_CHANNEL_MAXLEN) {
            ZCM_DEBUG("bad channel name length");
            udp_discarded_bad++;
            loss.markDropped(from, msg_seqno);
            return NULL;
        }

        fbuf = pool.addFragBuf(channel_sz + 1 + data_size);
        fbuf->last_packet_utime = pkt->utime;
        fbuf->msg_seqno = msg_seqno;
        fbuf->fragments_remaining = fragments_in_msg;
        fbuf->channellen = channel_sz;
        fbuf->from = *from;
        memcpy(fbuf->buf.data, data_start, frag_size);

        --fbuf->fragments_remaining;
        return NULL;
    }

    if (!fbuf) {
        // A fragment of a message whose first fragment we never saw (or
        // that was already discarded): it can't be reassembled.
        loss.markDropped(from, msg_seqno);
        return NULL;
    }
    recvfd.checkAndWarnAboutSmallBuffer(data_size, kernel_rbuf_sz);

    if (fbuf->channellen+1 + fragment_offset + frag_size > fbuf->buf.size) {
        ZCM_DEBUG("dropping invalid fragment (off: %d, %d / %zu)",
                fragment_offset, frag_size, fbuf->buf.size);
        loss.markDropped(from, msg_seqno);
        pool.removeFragBuf(fbuf);
        return NULL;
    }

    // copy data
    memcpy(fbuf->buf.data + fbuf->channellen+1 + fragment_offset, data_start, frag_size);

    fbuf->last_packet_utime = pkt->utime;
    if (--fbuf->fragments_remaining > 0)
        return NULL;

    // we've received all the fragments, return a new Message
    Message *msg = pool.allocMessageEmpty();
    msg->utime = fbuf->last_packet_utime;
    msg->channel = fbuf->buf.data;
    msg->channellen = fbuf->channellen;
    msg->data = fbuf->buf.data + fbuf->channellen + 1;
    msg->datalen = fbuf->buf.size - (fbuf->channellen + 1);
    pool.moveBuffer(msg->buf, fbuf->buf);

    udp_rx++;
    loss.markDelivered(from, msg_seqno);

    // don't need the fragment buffer anymore
    pool.removeFragBuf(fbuf);

    return msg;
}

void UDP::checkForMessageLoss()
{
    // Periodically summarize loss when debugging is enabled (ZCM_DEBUG=1)
#ifdef ZCM_DEBUG_ENV_ENABLE
    if (!ZCM_DEBUG_ENABLED) return;
#else
    return;
#endif

    i32 tm = utimeInSeconds();
    if (tm - udp_last_report_secs < 2) return;

    u64 drops = loss.getDrops();
    u64 kdrops = 0;
    bool haveKernelDrops = recvfd.getKernelDrops(kdrops);
    if (drops != udp_last_report_drops || kdrops != udp_last_report_kernel_drops) {
        char kernelMsg[96] = "";
        if (haveKernelDrops)
            snprintf(kernelMsg, sizeof(kernelMsg),
                     ", %" PRIu64 " packets dropped by the kernel socket buffer", kdrops);
        ZCM_DEBUG("UDP loss: %" PRIu64 " messages dropped (+%" PRIu64 "), "
                  "%" PRIu64 " messages received, %" PRIu64 " bad packets%s",
                  drops, drops - udp_last_report_drops,
                  udp_rx, udp_discarded_bad, kernelMsg);
        udp_last_report_drops = drops;
        udp_last_report_kernel_drops = kdrops;
    }
    udp_last_report_secs = tm;
}

// read continuously until a complete message arrives
Message *UDP::readMessage(unsigned timeoutMs)
{
    Packet *pkt = pool.allocPacket(ZCM_MAX_UNFRAGMENTED_PACKET_SIZE);
    UDP::checkForMessageLoss();

    Message *msg = NULL;
    while (!msg) {
        // // wait for either incoming UDP data, or for an abort message
        if (!recvfd.waitUntilData(timeoutMs)) break;

        int sz = recvfd.recvPacket(pkt);
        if (sz < 0) {
            ZCM_DEBUG("udp_read_packet -- recvmsg");
            udp_discarded_bad++;
            continue;
        }

        ZCM_DEBUG("Got packet of size %d", sz);

        if (sz < (int)sizeof(MsgHeaderShort)) {
            // packet too short to be ZCM
            udp_discarded_bad++;
            continue;
        }

        u32 magic = pkt->asHeaderShort()->getMagic();
        if (magic == ZCM_MAGIC_SHORT)
            msg = recvShort(pkt, sz);
        else if (magic == ZCM_MAGIC_LONG)
            msg = recvFragment(pkt, sz);
        else {
            ZCM_DEBUG("ZCM: bad magic");
            udp_discarded_bad++;
            continue;
        }
    }

    pool.freePacket(pkt);
    return msg;
}

int UDP::sendmsg(zcm_msg_t msg)
{
    int channel_size = strlen(msg.channel);
    if (channel_size > ZCM_CHANNEL_MAXLEN) {
        fprintf(stderr, "ZCM Error: channel name too long [%s]\n", msg.channel);
        return ZCM_EINVALID;
    }

    int payload_size = channel_size + 1 + msg.len;
    if (payload_size <= ZCM_SHORT_MESSAGE_MAX_SIZE) {
        // message is short.  send in a single packet

        MsgHeaderShort hdr;
        hdr.setMagic(ZCM_MAGIC_SHORT);
        hdr.setMsgSeqno(msg_seqno);

        ssize_t status = sendfd.sendBuffers(destAddr,
                              (char*)&hdr, sizeof(hdr),
                              (char*)msg.channel, channel_size+1,
                              (char*)msg.buf, msg.len);

        int packet_size = sizeof(hdr) + payload_size;
        ZCM_DEBUG("transmitting %zu byte [%s] payload (%d byte pkt) status: %zd",
                  msg.len, msg.channel, packet_size, status);
        msg_seqno++;

        return (status == packet_size) ? 0 : status;
    }


    else {
        // message is large.  fragment into multiple packets
        int fragment_size = ZCM_FRAGMENT_MAX_PAYLOAD;
        int nfragments = payload_size / fragment_size +
            !!(payload_size % fragment_size);

        if (nfragments > 65535) {
            fprintf(stderr, "ZCM error: too much data for a single message\n");
            return -1;
        }

        // acquire transmit lock so that all fragments are transmitted
        // together, and so that no other message uses the same sequence number
        // (at least until the sequence # rolls over)

        ZCM_DEBUG("transmitting %d byte [%s] payload in %d fragments",
                  payload_size, msg.channel, nfragments);

        u32 fragment_offset = 0;

        MsgHeaderLong hdr;
        hdr.magic = htonl(ZCM_MAGIC_LONG);
        hdr.msg_seqno = htonl(msg_seqno);
        hdr.msg_size = htonl(msg.len);
        hdr.fragment_offset = 0;
        hdr.fragment_no = 0;
        hdr.fragments_in_msg = htons(nfragments);

        // first fragment is special.  insert channel before data
        size_t firstfrag_datasize = fragment_size - (channel_size + 1);
        assert(firstfrag_datasize <= msg.len);

        int packet_size = sizeof(hdr) + (channel_size + 1) + firstfrag_datasize;
        fragment_offset += firstfrag_datasize;

        ssize_t status = sendfd.sendBuffers(destAddr,
                                            (char*)&hdr, sizeof(hdr),
                                            (char*)msg.channel, channel_size+1,
                                            (char*)msg.buf, firstfrag_datasize);

        // transmit the rest of the fragments
        for (u16 frag_no = 1; packet_size == status && frag_no < nfragments; frag_no++) {
            hdr.fragment_offset = htonl(fragment_offset);
            hdr.fragment_no = htons(frag_no);

            int fraglen = std::min(fragment_size, (int)msg.len - (int)fragment_offset);
            status = sendfd.sendBuffers(destAddr,
                                        (char*)&hdr, sizeof(hdr),
                                        (char*)(msg.buf + fragment_offset), fraglen);

            fragment_offset += fraglen;
            packet_size = sizeof(hdr) + fraglen;
        }

        // sanity check
        if (0 == status) {
            assert(fragment_offset == msg.len);
        }

        msg_seqno++;
    }

    return 0;
}

int UDP::queryDrops(uint64_t *out_drops)
{
    if (!out_drops) return ZCM_EINVALID;
    *out_drops = loss.getDrops();
    return ZCM_EOK;
}

int UDP::recvmsg(zcm_msg_t *msg, unsigned timeoutMs)
{
    if (m) pool.freeMessage(m);

    m = readMessage(timeoutMs);
    if (m == nullptr)
        return ZCM_EAGAIN;

    msg->utime = m->utime;
    msg->channel = m->channel;
    msg->len = m->datalen;
    msg->buf = (uint8_t*) m->data;

    return ZCM_EOK;
}

UDP::~UDP()
{
    if (m) pool.freeMessage(m);
    ZCM_DEBUG("closing zcm context");
}

UDP::UDP(const string& ip, u16 sub_port, u16 pub_port,
         size_t recv_buf_size, u8 ttl, bool multicast)
    : params(ip, sub_port, pub_port, recv_buf_size, ttl, multicast),
      destAddr(ip, pub_port)
{
    pool.onEvict = [this](const FragBuf *fbuf) {
        ZCM_DEBUG("Evicting incomplete message (missing %d fragments)",
                  fbuf->fragments_remaining);
        loss.markDropped(&fbuf->from, fbuf->msg_seqno);
    };
}

bool UDP::init()
{
    ZCM_DEBUG("Initializing ZCM UDP context...");
    UDPSocket::checkConnection(params.ip, params.sub_port);
    if (params.multicast) {
        ZCM_DEBUG("Multicast %s:%d", params.ip.c_str(), params.sub_port);
    } else {
        UDPSocket::checkConnection(params.ip, params.pub_port);
    }

    sendfd = UDPSocket::createSendSocket(params.addr, params.ttl, params.multicast);
    if (!sendfd.isOpen()) return false;
    kernel_sbuf_sz = sendfd.getSendBufSize();

    recvfd = UDPSocket::createRecvSocket(params.addr, params.sub_port, params.multicast);
    if (!recvfd.isOpen()) return false;
    kernel_rbuf_sz = recvfd.getRecvBufSize();

    if (!this->selftest()) {
        // self test failed.  destroy the read thread
        fprintf(stderr, "ZCM self test failed!!\n"
                "Check your routing and firewall settings\n");
        return false;
    }

    return true;
}

bool UDP::selftest()
{
#ifdef ENABLE_SELFTEST
    ZCM_DEBUG("UDP conducting self test");
    assert(0 && "unimpl");
#endif
    return true;
}

// Define this the class name you want
#define ZCM_TRANS_CLASSNAME TransportUDP

struct ZCM_TRANS_CLASSNAME : public zcm_trans_t
{
    UDP udp;

    ZCM_TRANS_CLASSNAME(const string& ip, u16 sub_port, u16 pub_port, size_t recv_buf_size,
                        u8 ttl, bool isMulticast)
        : udp(ip, sub_port, pub_port, recv_buf_size, ttl, isMulticast)
    {
        trans_type = ZCM_BLOCKING;
        vtbl = &methods;
    }

    bool init() { return udp.init(); }

    /********************** STATICS **********************/
    static zcm_trans_methods_t methods;
    static ZCM_TRANS_CLASSNAME *cast(zcm_trans_t *zt)
    {
        assert(zt->vtbl == &methods);
        return (ZCM_TRANS_CLASSNAME*)zt;
    }

    static size_t _getMtu(zcm_trans_t *zt)
    { return MTU; }

    static int _sendmsg(zcm_trans_t *zt, zcm_msg_t msg)
    { return cast(zt)->udp.sendmsg(msg); }

    static int _recvmsgEnable(zcm_trans_t *zt, const char *channel, bool enable)
    { return ZCM_EOK; }

    static int _recvmsg(zcm_trans_t *zt, zcm_msg_t *msg, unsigned timeout)
    { return cast(zt)->udp.recvmsg(msg, timeout); }

    static int _queryDrops(zcm_trans_t *zt, uint64_t *out_drops)
    { return cast(zt)->udp.queryDrops(out_drops); }

    static void _destroy(zcm_trans_t *zt)
    { delete cast(zt); }

    static const TransportRegister regUdpm;
    static const TransportRegister regUdp;
};

zcm_trans_methods_t ZCM_TRANS_CLASSNAME::methods = {
    &ZCM_TRANS_CLASSNAME::_getMtu,
    &ZCM_TRANS_CLASSNAME::_sendmsg,
    &ZCM_TRANS_CLASSNAME::_recvmsgEnable,
    &ZCM_TRANS_CLASSNAME::_recvmsg,
    &ZCM_TRANS_CLASSNAME::_queryDrops,
    NULL, // update
    &ZCM_TRANS_CLASSNAME::_destroy,
};

static const char *optFind(zcm_url_opts_t *opts, const string& key)
{
    for (size_t i = 0; i < opts->numopts; i++)
        if (key == opts->name[i])
            return opts->value[i];
    return NULL;
}

static zcm_trans_t *createUdp(zcm_url_t *url, char **opt_errmsg)
{
    if (opt_errmsg) *opt_errmsg = NULL; // Feature unused in this transport

    auto protocol = string(zcm_url_protocol(url));
    bool isMulticast = protocol == "udpm";

    auto *ip = zcm_url_address(url);

    vector<string> parts = StringUtil::split(ip, ':');

    string subPort, pubPort;

    if (isMulticast) {
        if (parts.size() != 2) {
            ZCM_DEBUG("ERROR: Url format is <ip-address>:<port-num>");
            return nullptr;
        }
        subPort = pubPort = parts[1];
    } else {
        if (parts.size() != 3) {
            ZCM_DEBUG("ERROR: Url format is <ip-address>:<sub-port-num>:<pub-port-num>");
            return nullptr;
        }
        subPort = parts[1];
        pubPort = parts[2];
    }
    auto& address = parts[0];

    auto *opts = zcm_url_opts(url);
    auto *ttl = optFind(opts, "ttl");
    if (!ttl) {
        ttl = isMulticast ? "0" : "1";
        ZCM_DEBUG("No ttl specified. Using default ttl=%s", ttl);
    }
    size_t recv_buf_size = 1024;
    auto *trans = new ZCM_TRANS_CLASSNAME(address,
                                          atoi(subPort.c_str()), atoi(pubPort.c_str()),
                                          recv_buf_size, atoi(ttl), isMulticast);
    if (!trans->init()) {
        delete trans;
        return nullptr;
    } else {
        return trans;
    }
}

#ifdef USING_TRANS_UDP
// Register this transport with ZCM
const TransportRegister ZCM_TRANS_CLASSNAME::regUdpm(
    "udpm", "Transfer data via UDP Multicast (e.g. 'udpm')", createUdp);
// Register this transport with ZCM
const TransportRegister ZCM_TRANS_CLASSNAME::regUdp(
    "udp", "Transfer data via UDP Unicast (e.g. 'udp')", createUdp);
#endif
