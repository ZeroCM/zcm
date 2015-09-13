#include "udpm.hpp"
#include "buffers.hpp"
#include "udpmsocket.hpp"
#include "mempool.hpp"

#include "zcm/transport.h"
#include "zcm/transport_registrar.h"

#define MTU (1<<28)

static i32 utimeInSeconds()
{
    struct timeval tv;
    gettimeofday (&tv, NULL);
    return (i32)tv.tv_sec;
}

/**
 * udpm_params_t:
 * @mc_addr:        multicast address
 * @mc_port:        multicast port
 * @mc_ttl:         if 0, then packets never leave local host.
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
    u16            port;
    u8             ttl;
    size_t         recv_buf_size;

    Params(const string& ip, u16 port, size_t recv_buf_size, u8 ttl)
    {
        // TODO verify that the IP and PORT are vaild
        this->ip = ip;
        inet_aton(ip.c_str(), (struct in_addr*) &this->addr);
        this->port = port;
        this->recv_buf_size = recv_buf_size;
        this->ttl = ttl;
    }
};

struct UDPM
{
    Params params;
    UDPMAddress destAddr;

    UDPMSocket recvfd;
    UDPMSocket sendfd;

    /* size of the kernel UDP receive buffer */
    size_t kernel_rbuf_sz = 0;
    size_t kernel_sbuf_sz = 0;
    bool warned_about_small_kernel_buf = false;

    MessagePool pool {MAX_FRAG_BUF_TOTAL_SIZE, MAX_NUM_FRAG_BUFS};

    /* other variables */
    u32          udp_rx = 0;            // packets received and processed
    u32          udp_discarded_bad = 0; // packets discarded because they were bad
                                    // somehow
    double       udp_low_watermark = 1.0; // least buffer available
    i32          udp_last_report_secs = 0;

    u32          msg_seqno = 0; // rolling counter of how many messages transmitted

    /***** Methods ******/
    UDPM(const string& ip, u16 port, size_t recv_buf_size, u8 ttl);
    bool init();
    ~UDPM();

    int handle();

    int sendmsg(zcm_msg_t msg);
    int recvmsg(zcm_msg_t *msg, int timeout);

  private:
    // These return true when a full message has been received
    bool _recv_fragment(Message *zcmb, u32 sz);
    bool _recv_short(Message *zcmb, u32 sz);
    Message *udp_read_packet();

    bool selftest();
    void checkForMessageLoss();
};

bool UDPM::_recv_fragment(Message *zcmb, u32 sz)
{
    MsgHeaderLong *hdr = (MsgHeaderLong*) zcmb->buf.data;

    // any existing fragment buffer for this message source?
    FragBuf *fbuf = pool.lookupFragBuf((struct sockaddr_in*)&zcmb->from);

    u32 msg_seqno = ntohl(hdr->msg_seqno);
    u32 data_size = ntohl(hdr->msg_size);
    u32 fragment_offset = ntohl(hdr->fragment_offset);
    u16 fragment_no = ntohs(hdr->fragment_no);
    u16 fragments_in_msg = ntohs(hdr->fragments_in_msg);
    u32 frag_size = sz - sizeof(MsgHeaderLong);
    char *data_start = (char*)(hdr+1);

    // discard any stale fragments from previous messages
    if (fbuf && ((fbuf->msg_seqno != msg_seqno) ||
                 (fbuf->buf.size != data_size))) {
        pool.removeFragBuf(fbuf);
        ZCM_DEBUG("Dropping message (missing %d fragments)", fbuf->fragments_remaining);
        fbuf = NULL;
    }

    if (data_size > MTU) {
        ZCM_DEBUG("rejecting huge message (%d bytes)", data_size);
        return false;
    }

    // create a new fragment buffer if necessary
    if (!fbuf && fragment_no == 0) {
        char *channel = (char*) (hdr + 1);
        int channel_sz = strlen (channel);
        if (channel_sz > ZCM_CHANNEL_MAXLEN) {
            ZCM_DEBUG("bad channel name length");
            udp_discarded_bad++;
            return false;
        }

        // if the packet has no subscribers, drop the message now.
        // XXX add this back
        // if(!zcm_has_handlers(zcm, channel))
        //     return 0;

        fbuf = pool.addFragBuf(data_size);
        strncpy(fbuf->channel, channel, sizeof(fbuf->channel));
        fbuf->channel[sizeof(fbuf->channel)-1] = '\0';
        fbuf->from = *(struct sockaddr_in*)&zcmb->from;
        fbuf->msg_seqno = msg_seqno;
        fbuf->fragments_remaining = fragments_in_msg;
        fbuf->last_packet_utime = zcmb->recv_utime;

        data_start += channel_sz + 1;
        frag_size -= (channel_sz + 1);
    }

    if (!fbuf) return false;

#ifdef __linux__
    if (kernel_rbuf_sz < 262145 && data_size > kernel_rbuf_sz &&
        !warned_about_small_kernel_buf)
    {
        warned_about_small_kernel_buf = true;
        fprintf(stderr,
"==== ZCM Warning ===\n"
"ZCM detected that large packets are being received, but the kernel UDP\n"
"receive buffer is very small.  The possibility of dropping packets due to\n"
"insufficient buffer space is very high.\n"
"\n"
"For more information, visit:\n"
"   http://zcm-proj.github.io/multicast_setup.html\n\n");
    }
#endif

    if (fragment_offset + frag_size > fbuf->buf.size) {
        ZCM_DEBUG("dropping invalid fragment (off: %d, %d / %zu)",
                fragment_offset, frag_size, fbuf->buf.size);
        pool.removeFragBuf(fbuf);
        return false;
    }

    // copy data
    memcpy (fbuf->buf.data + fragment_offset, data_start, frag_size);
    fbuf->last_packet_utime = zcmb->recv_utime;

    fbuf->fragments_remaining --;

    if (fbuf->fragments_remaining > 0)
        return false;

    // transfer ownership of the message's payload buffer
    pool.transferBufffer(zcmb, fbuf);

    strcpy(zcmb->channel_name, fbuf->channel);
    zcmb->channel_size = strlen(zcmb->channel_name);
    zcmb->data_offset = 0;
    zcmb->data_size = fbuf->buf.size;
    zcmb->recv_utime = fbuf->last_packet_utime;

    // don't need the fragment buffer anymore
    pool.removeFragBuf(fbuf);

    return true;
}

bool UDPM::_recv_short(Message *zcmb, u32 sz)
{
    MsgHeaderShort *hdr = (MsgHeaderShort*) zcmb->buf.data;

    // shouldn't have to worry about buffer overflow here because we
    // zeroed out byte #65536, which is never written to by recv
    const char *pkt_channel_str = (char*)(hdr+1);
    zcmb->channel_size = strlen(pkt_channel_str);
    if (zcmb->channel_size > ZCM_CHANNEL_MAXLEN) {
        ZCM_DEBUG("bad channel name length");
        udp_discarded_bad++;
        return false;
    }

     udp_rx++;

    strcpy(zcmb->channel_name, pkt_channel_str);
    zcmb->data_offset = sizeof(MsgHeaderShort) + zcmb->channel_size + 1;
    zcmb->data_size = sz - zcmb->data_offset;

    return true;
}

void UDPM::checkForMessageLoss()
{
    // XXX add this back
    // TODO warn about message loss somewhere else.
    // u32 ring_capacity = ringbuf->get_capacity();
    // u32 ring_used = ringbuf->get_used();

    // double buf_avail = ((double)(ring_capacity - ring_used)) / ring_capacity;
    // if (buf_avail < udp_low_watermark)
    //     udp_low_watermark = buf_avail;

    // i32 tm = utimeInSeconds();
    // int elapsedsecs = tm - udp_last_report_secs;
    // if (elapsedsecs > 2) {
    //    if (udp_discarded_bad > 0 || udp_low_watermark < 0.5) {
    //        fprintf(stderr,
    //                "%d ZCM loss %4.1f%% : %5d err, "
    //                "buf avail %4.1f%%\n",
    //                (int) tm,
    //                udp_discarded_bad * 100.0 / (udp_rx + udp_discarded_bad),
    //                udp_discarded_bad,
    //                100.0 * udp_low_watermark);

    //        udp_rx = 0;
    //        udp_discarded_bad = 0;
    //        udp_last_report_secs = tm;
    //        udp_low_watermark = HUGE;
    //    }
    // }
}

// read continuously until a complete message arrives
Message *UDPM::udp_read_packet()
{
    Message *zcmb = NULL;
    size_t sz = 0;

    UDPM::checkForMessageLoss();

    bool got_complete_message = false;
    while (!got_complete_message) {
        // // wait for either incoming UDP data, or for an abort message
        if (!recvfd.waitUntilData())
            continue;

        if (!zcmb)
            zcmb = pool.allocMessage();

        sz = recvfd.recvBuffer(zcmb);
        if (sz < 0) {
            ZCM_DEBUG("udp_read_packet -- recvmsg");
            udp_discarded_bad++;
            continue;
        }

        ZCM_DEBUG("Got packet of size %zu", sz);

        if (sz < sizeof(MsgHeaderShort)) {
            // packet too short to be ZCM
            udp_discarded_bad++;
            continue;
        }

        MsgHeaderShort *hdr = (MsgHeaderShort*) zcmb->buf.data;
        u32 rcvd_magic = ntohl(hdr->magic);
        if (rcvd_magic == ZCM_MAGIC_SHORT)
            got_complete_message = _recv_short(zcmb, sz);
        else if (rcvd_magic == ZCM_MAGIC_LONG)
            got_complete_message = _recv_fragment(zcmb, sz);
        else {
            ZCM_DEBUG("ZCM: bad magic");
            udp_discarded_bad++;
            continue;
        }
    }

    return zcmb;
}

int UDPM::sendmsg(zcm_msg_t msg)
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
        hdr.magic = htonl(ZCM_MAGIC_SHORT);
        hdr.msg_seqno = htonl(msg_seqno);

        ssize_t status = sendfd.sendBuffers(destAddr,
                              (char*)&hdr, sizeof(hdr),
                              (char*)msg.channel, channel_size+1,
                              msg.buf, msg.len);

        int packet_size = sizeof(hdr) + payload_size;
        ZCM_DEBUG("transmitting %zu byte [%s] payload (%d byte pkt)",
                  msg.len, msg.channel, packet_size);
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
                                            msg.buf, firstfrag_datasize);

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

int UDPM::recvmsg(zcm_msg_t *msg, int timeout)
{
    static Message *m = NULL;
    if (m)
        pool.freeMessage(m);

    m = udp_read_packet();
    if (m == nullptr)
        return ZCM_EAGAIN;

   msg->channel = m->channel_name;
   msg->len = m->data_size;
   msg->buf = m->buf.data + m->data_offset;

   return ZCM_EOK;
}

UDPM::~UDPM()
{
    ZCM_DEBUG("closing zcm context");
}

UDPM::UDPM(const string& ip, u16 port, size_t recv_buf_size, u8 ttl)
    : params(ip, port, recv_buf_size, ttl),
      destAddr(ip, port)
{
}

bool UDPM::init()
{
    ZCM_DEBUG("Initializing ZCM UDPM context...");
    ZCM_DEBUG("Multicast %s:%d", params.ip.c_str(), params.port);
    UDPMSocket::checkConnection(params.ip, params.port);

    sendfd = UDPMSocket::createSendSocket(params.addr, params.ttl);
    if (!sendfd.isOpen()) return false;
    kernel_sbuf_sz = sendfd.getSendBufSize();

    recvfd = UDPMSocket::createRecvSocket(params.addr, params.port);
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

bool UDPM::selftest()
{
#ifdef ENABLE_SELFTEST
    ZCM_DEBUG("UDPM conducting self test");
    assert(0 && "unimpl");
#endif
    return true;
}

// Define this the class name you want
#define ZCM_TRANS_CLASSNAME TransportUDPM

struct ZCM_TRANS_CLASSNAME : public zcm_trans_t
{
    UDPM udpm;

    ZCM_TRANS_CLASSNAME(const string& ip, u16 port, size_t recv_buf_size, u8 ttl)
        : udpm(ip, port, recv_buf_size, ttl)
    {
        vtbl = &methods;
    }

    bool init() { return udpm.init(); }

    /********************** STATICS **********************/
    static zcm_trans_methods_t methods;
    static ZCM_TRANS_CLASSNAME *cast(zcm_trans_t *zt)
    {
        assert(zt->vtbl == &methods);
        return (ZCM_TRANS_CLASSNAME*)zt;
    }

    static size_t _getMtu(zcm_trans_t *zt)
    { cast(zt); return MTU; }

    static int _sendmsg(zcm_trans_t *zt, zcm_msg_t msg)
    { return cast(zt)->udpm.sendmsg(msg); }

    static int _recvmsgEnable(zcm_trans_t *zt, const char *channel, bool enable)
    { cast(zt); return ZCM_EOK; }

    static int _recvmsg(zcm_trans_t *zt, zcm_msg_t *msg, int timeout)
    { return cast(zt)->udpm.recvmsg(msg, timeout); }

    static void _destroy(zcm_trans_t *zt)
    { delete cast(zt); }
};

zcm_trans_methods_t ZCM_TRANS_CLASSNAME::methods = {
    &ZCM_TRANS_CLASSNAME::_getMtu,
    &ZCM_TRANS_CLASSNAME::_sendmsg,
    &ZCM_TRANS_CLASSNAME::_recvmsgEnable,
    &ZCM_TRANS_CLASSNAME::_recvmsg,
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

static zcm_trans_t *createUdpm(zcm_url_t *url)
{
    auto *ip = zcm_url_address(url);
    auto *opts = zcm_url_opts(url);
    auto *port = optFind(opts, "port");
    auto *ttl = optFind(opts, "ttl");
    size_t recv_buf_size = 1024;
    auto *trans = new ZCM_TRANS_CLASSNAME(ip, atoi(port), recv_buf_size, atoi(ttl));
    if (!trans->init()) {
        delete trans;
        return nullptr;
    } else {
        return trans;
    }
}

// Register this transport with ZCM
static struct Register { Register() {
    zcm_transport_register("udpm",    "Transfer data via UDP Multicast (e.g. 'udpm')", createUdpm);
}} reg;
