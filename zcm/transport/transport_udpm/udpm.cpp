#include "udpm.hpp"
#include "fragbuffer.hpp"

#include "zcm/transport.h"
#include "zcm/transport_registrar.h"

struct BufferSizes
{
    size_t send;
    size_t recv;
};

static BufferSizes getKernelBuffers(int fd)
{
    BufferSizes bsz;

    // how big is the send buffer?
    {
        int bufsize = 0;
        unsigned int retsize = sizeof(int);
        getsockopt(fd, SOL_SOCKET, SO_SNDBUF,
                   (char*)&bufsize, (socklen_t*)&retsize);
        ZCM_DEBUG("ZCM: send buffer is %d bytes", bufsize);
        bsz.send = bufsize;
    }

    // how big is the receive buffer?
    {
        int bufsize = 0;
        unsigned int retsize = sizeof (int);
        getsockopt(fd, SOL_SOCKET, SO_RCVBUF,
                   (char*)&bufsize, (socklen_t *)&retsize);
        ZCM_DEBUG("ZCM: receive buffer is %d bytes", bufsize);
        bsz.recv = bufsize;
    }

    return bsz;
};

// Platform specifics
#ifdef WIN32
struct Platform
{
    static void closesocket(int fd) { closesocket(fd); }
    static void setKernelBuffers(int fd)
    {
        // Windows has small (8k) buffer by default
        // increase the send buffer to a reasonable amount.
        int send_size = 256 * 1024;
        int recv_size = 2048 * 1024;

        setsockopt(fd, SOL_SOCKET, SO_SNDBUF, (char*)&send_size, sizeof(send_size));
        setsockopt(fd, SOL_SOCKET, SO_RCVBUF, (char*)&recv_size, sizeof(recv_size));
    }

    static bool setMulticastGroup(int fd, struct in_addr multiaddr)
    {
        struct ip_mreq mreq;
        mreq.imr_multiaddr = multiaddr;
        mreq.imr_interface.s_addr = INADDR_ANY;
        ZCM_DEBUG("ZCM: joining multicast group");
        setsockopt(fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char*)&mreq, sizeof(mreq));
        // ignore any errors in windows... see issue LCM #60
        return true;
    }
};
#else
struct Platform
{
    static void closesocket(int fd) { close(fd); }
    static void setKernelBuffers(int fd) {}
    static bool setMulticastGroup(int fd, struct in_addr multiaddr)
    {
        struct ip_mreq mreq;
        mreq.imr_multiaddr = multiaddr;
        mreq.imr_interface.s_addr = INADDR_ANY;
        ZCM_DEBUG("ZCM: joining multicast group");
        int ret = setsockopt(fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char*)&mreq, sizeof(mreq));
        if (ret < 0) {
            perror("setsockopt (IPPROTO_IP, IP_ADD_MEMBERSHIP)");
            return false;
        }
        return true;
    }
};
#endif

class UDPMSocket
{
  public:
    ~UDPMSocket() { close(); }

    bool isOpen() { return fd != -1; }

    bool init()
    {
        fd = socket(AF_INET, SOCK_DGRAM, 0);
        if (fd < 0) {
            perror("allocating ZCM udpm socket");
            return false;
        }
        return true;
    }

    bool joinMulticastGroup(struct in_addr multiaddr)
    {
        //Platform::setKernelBuffers(fd);
        //bsz = getKernelBuffers(fd);

        // NOTE: For support on SUN Operating Systems, send_lo_opt should be 'u8'
        //       We don't currently support SUN
        // u32 send_lo_opt = 1;
        // if (setsockopt(fd, IPPROTO_IP, IP_MULTICAST_LOOP,
        //                (char *)&send_lo_opt, sizeof (send_lo_opt)) < 0) {
        //     perror("setsockopt (IPPROTO_IP, IP_MULTICAST_LOOP)");
        //     this->close();
        //     return false;
        // }

        // Set-up the multicast group
        if (!Platform::setMulticastGroup(fd, multiaddr)) {
            this->close();
            return false;
        }

        return true;
    }

    bool setTTL(u8 ttl)
    {
        if (ttl == 0)
            ZCM_DEBUG("ZCM multicast TTL set to 0.  Packets will not leave localhost");

        ZCM_DEBUG("ZCM: setting multicast packet TTL to %d", ttl);
        if (setsockopt(fd, IPPROTO_IP, IP_MULTICAST_TTL,
                       (char *) &ttl, sizeof (ttl)) < 0) {
            perror("setsockopt(IPPROTO_IP, IP_MULTICAST_TTL)");
            return false;
        }

        return true;
    }

    bool bindPort(u16 port)
    {
        struct sockaddr_in addr;
        memset(&addr, 0, sizeof (addr));
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;
        addr.sin_port = port;

        if (bind(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
            perror("bind");
            return false;
        }
        return true;
    }

    bool setReuseAddr()
    {
        // allow other applications on the local machine to also bind to this
        // multicast address and port
        int opt = 1;
        ZCM_DEBUG("ZCM: setting SO_REUSEADDR");
        if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR,
                       (char*)&opt, sizeof (opt)) < 0) {
            perror ("setsockopt (SOL_SOCKET, SO_REUSEADDR)");
            return false;
        }
        return true;
    }

    bool setReusePort()
    {
#ifdef USE_REUSEPORT
        /* Mac OS and FreeBSD require the REUSEPORT option in addition
         * to REUSEADDR or it won't let multiple processes bind to the
         * same port, even if they are using multicast. */
        int opt = 1;
        ZCM_DEBUG("ZCM: setting SO_REUSEPORT");
        if (setsockopt(fd, SOL_SOCKET, SO_REUSEPORT,
                       (char*)&opt, sizeof (opt)) < 0) {
            perror("setsockopt (SOL_SOCKET, SO_REUSEPORT)");
            return false;
        }
#endif
        return true;
    }

    bool enablePacketTimestamp()
    {
        /* Enable per-packet timestamping by the kernel, if available */
#ifdef SO_TIMESTAMP
        int opt = 1;
        setsockopt(fd, SOL_SOCKET, SO_TIMESTAMP, &opt, sizeof (opt));
#endif
        return true;
    }

    void close()
    {
        if (fd != -1) {
            Platform::closesocket(fd);
            fd = -1;
        }
    }

    static UDPMSocket createSendSocket(struct in_addr multiaddr, u8 ttl)
    {
        // don't use connect() on the actual transmit socket, because linux then
        // has problems multicasting to localhost
        UDPMSocket sock;
        if (!sock.init())                        { sock.close(); return sock; }
        if (!sock.setTTL(ttl))                   { sock.close(); return sock; }
        if (!sock.joinMulticastGroup(multiaddr)) { sock.close(); return sock; }
        return sock;
    }

    static UDPMSocket createRecvSocket(struct in_addr multiaddr, u16 port)
    {
        UDPMSocket sock;
        if (!sock.init())                        { sock.close(); return sock; }
        if (!sock.setReuseAddr())                { sock.close(); return sock; }
        if (!sock.setReusePort())                { sock.close(); return sock; }
        if (!sock.enablePacketTimestamp())       { sock.close(); return sock; }
        if (!sock.bindPort(port))                { sock.close(); return sock; }
        if (!sock.joinMulticastGroup(multiaddr)) { sock.close(); return sock; }
        return sock;
    }

  //private:
    SOCKET fd = -1;
    BufferSizes bsz;
};


#define MTU (1<<20)

static i32 utimeInSeconds()
{
    struct timeval tv;
    gettimeofday (&tv, NULL);
    return (i32)tv.tv_sec;
}

static i64 timestampNow()
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (i64)tv.tv_sec * 1000000 + tv.tv_usec;
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
    struct in_addr addr;
    u16            port;
    u8             ttl;
    size_t         recv_buf_size;

    Params(const string& ip, u16 port, size_t recv_buf_size, u8 ttl)
    {
        // TODO verify that the IP and PORT are vaild
        inet_aton(ip.c_str(), (struct in_addr*) &this->addr);
        this->port = port;
        this->recv_buf_size = recv_buf_size;
        this->ttl = ttl;
    }
};

struct UDPM
{
    UDPMSocket recvfd;
    UDPMSocket sendfd;
    struct sockaddr_in dest_addr;

    Params params;

    /* size of the kernel UDP receive buffer */
    size_t kernel_rbuf_sz = 0;
    bool warned_about_small_kernel_buf = false;

    /* Packet structures available for sending or receiving use are
     * stored in the *_empty queues. */
    BufQueue *inbufs_empty = nullptr;
    /* Received packets that are filled with data are queued here. */
    BufQueue *inbufs_filled = nullptr;

    /* Memory for received small packets is taken from a fixed-size ring buffer
     * so we don't have to do any mallocs */
    Ringbuffer *ringbuf;

    std::mutex mut; /* Must be locked when reading/writing to the
                       above three queues */

    /* other variables */
    FragBufStore *frag_bufs = nullptr;

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
    bool _recv_fragment(Buffer *zcmb, u32 sz);
    bool _recv_short(Buffer *zcmb, u32 sz);

    Buffer *udp_read_packet();
    void recv_thread();
};

bool UDPM::_recv_fragment(Buffer *zcmb, u32 sz)
{
    MsgHeaderLong *hdr = (MsgHeaderLong*) zcmb->buf;

    // any existing fragment buffer for this message source?
    FragBuf *fbuf = frag_bufs->lookup((struct sockaddr_in*)&zcmb->from);

    u32 msg_seqno = ntohl(hdr->msg_seqno);
    u32 data_size = ntohl(hdr->msg_size);
    u32 fragment_offset = ntohl(hdr->fragment_offset);
    u16 fragment_no = ntohs(hdr->fragment_no);
    u16 fragments_in_msg = ntohs(hdr->fragments_in_msg);
    u32 frag_size = sz - sizeof(MsgHeaderLong);
    char *data_start = (char*)(hdr+1);

    // discard any stale fragments from previous messages
    if (fbuf && ((fbuf->msg_seqno != msg_seqno) ||
                 (fbuf->data_size != data_size))) {
        frag_bufs->remove(fbuf);
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

        fbuf = new FragBuf(*(struct sockaddr_in*)&zcmb->from,
                           channel, msg_seqno, data_size, fragments_in_msg,
                           zcmb->recv_utime);
        frag_bufs->add(fbuf);
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

    if (fragment_offset + frag_size > fbuf->data_size) {
        ZCM_DEBUG("dropping invalid fragment (off: %d, %d / %d)",
                fragment_offset, frag_size, fbuf->data_size);
        frag_bufs->remove(fbuf);
        return false;
    }

    // copy data
    memcpy (fbuf->data + fragment_offset, data_start, frag_size);
    fbuf->last_packet_utime = zcmb->recv_utime;

    fbuf->fragments_remaining --;

    if (fbuf->fragments_remaining > 0)
        return false;

    // XXX add this back
    // complete message received.  Is there a subscriber that still
    // wants it?  (i.e., does any subscriber have space in its queue?)
    // if(!zcm_try_enqueue_message(zcm->zcm, fbuf->channel)) {
    //     // no... sad... free the fragment buffer and return
    //     zcm_frag_buf_store_remove (zcm->frag_bufs, fbuf);
    //     return false;
    // }

    // yes, transfer the message into the zcm_buf_t

    // deallocate the ringbuffer-allocated buffer
    mut.lock();
    Buffer::destroy(zcmb, ringbuf);
    mut.unlock();

    // transfer ownership of the message's payload buffer
    zcmb->buf = fbuf->data;
    fbuf->data = NULL;

    strcpy(zcmb->channel_name, fbuf->channel);
    zcmb->channel_size = strlen(zcmb->channel_name);
    zcmb->data_offset = 0;
    zcmb->data_size = fbuf->data_size;
    zcmb->recv_utime = fbuf->last_packet_utime;

    // don't need the fragment buffer anymore
    frag_bufs->remove(fbuf);

    return true;
}

bool UDPM::_recv_short(Buffer *zcmb, u32 sz)
{
    MsgHeaderShort *hdr = (MsgHeaderShort*) zcmb->buf;

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

     // XXX Add this!
//     // if the packet has no subscribers, drop the message now.
//     if(!zcm_try_enqueue_message(zcm->zcm, pkt_channel_str))
//         return 0;

    strcpy(zcmb->channel_name, pkt_channel_str);
    zcmb->data_offset = sizeof(MsgHeaderShort) + zcmb->channel_size + 1;
    zcmb->data_size = sz - zcmb->data_offset;

    return true;
}

// read continuously until a complete message arrives
Buffer *UDPM::udp_read_packet()
{
    Buffer *zcmb = NULL;
    int sz = 0;

    // TODO warn about message loss somewhere else.

    mut.lock();
    u32 ring_capacity = ringbuf->get_capacity();
    u32 ring_used = ringbuf->get_used();
    mut.unlock();

    double buf_avail = ((double)(ring_capacity - ring_used)) / ring_capacity;
    if (buf_avail < udp_low_watermark)
        udp_low_watermark = buf_avail;

    i32 tm = utimeInSeconds();
    int elapsedsecs = tm - udp_last_report_secs;
    if (elapsedsecs > 2) {
       if (udp_discarded_bad > 0 || udp_low_watermark < 0.5) {
           fprintf(stderr,
                   "%d ZCM loss %4.1f%% : %5d err, "
                   "buf avail %4.1f%%\n",
                   (int) tm,
                   udp_discarded_bad * 100.0 / (udp_rx + udp_discarded_bad),
                   udp_discarded_bad,
                   100.0 * udp_low_watermark);

           udp_rx = 0;
           udp_discarded_bad = 0;
           udp_last_report_secs = tm;
           udp_low_watermark = HUGE;
       }
    }

    bool got_complete_message = false;
    while (!got_complete_message) {
        // wait for either incoming UDP data, or for an abort message
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(recvfd.fd, &fds);

        if (select(recvfd.fd + 1, &fds, NULL, NULL, NULL) <= 0) {
            perror("udp_read_packet -- select:");
            continue;
        }

        // there is incoming UDP data ready.
        assert(FD_ISSET(recvfd.fd, &fds));

        if (!zcmb) {
            mut.lock();
            zcmb = Buffer::make(inbufs_empty, &ringbuf);
            mut.unlock();
        }

        struct iovec        vec;
        vec.iov_base = zcmb->buf;
        vec.iov_len = 65535;

        struct msghdr msg;
        memset(&msg, 0, sizeof(struct msghdr));
        msg.msg_name = &zcmb->from;
        msg.msg_namelen = sizeof(struct sockaddr);
        msg.msg_iov = &vec;
        msg.msg_iovlen = 1;
#ifdef MSG_EXT_HDR
        // operating systems that provide SO_TIMESTAMP allow us to obtain more
        // accurate timestamps by having the kernel produce timestamps as soon
        // as packets are received.
        char controlbuf[64];
        msg.msg_control = controlbuf;
        msg.msg_controllen = sizeof (controlbuf);
        msg.msg_flags = 0;
#endif
        sz = ::recvmsg(recvfd.fd, &msg, 0);
        if (sz < 0) {
            perror("udp_read_packet -- recvmsg");
            udp_discarded_bad++;
            continue;
        }

        ZCM_DEBUG("Got packet of size %d", (int)sz);

        if (sz < sizeof(MsgHeaderShort)) {
            // packet too short to be ZCM
            udp_discarded_bad++;
            continue;
        }

        zcmb->fromlen = msg.msg_namelen;

        int got_utime = 0;
#ifdef SO_TIMESTAMP
        struct cmsghdr *cmsg = CMSG_FIRSTHDR(&msg);
        /* Get the receive timestamp out of the packet headers if possible */
        while (!zcmb->recv_utime && cmsg) {
            if (cmsg->cmsg_level == SOL_SOCKET &&
                    cmsg->cmsg_type == SCM_TIMESTAMP) {
                struct timeval *t = (struct timeval*) CMSG_DATA (cmsg);
                zcmb->recv_utime = (int64_t) t->tv_sec * 1000000 + t->tv_usec;
                got_utime = 1;
                break;
            }
            cmsg = CMSG_NXTHDR (&msg, cmsg);
        }
#endif

        if (!got_utime)
            zcmb->recv_utime = timestampNow();

        MsgHeaderShort *hdr = (MsgHeaderShort*) zcmb->buf;
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

    // if the newly received packet is a short packet, then resize the space
    // allocated to it on the ringbuffer to exactly match the amount of space
    // required.  That way, we do not use 64k of the ringbuffer for every
    // incoming message.
    if (ringbuf) {
        mut.lock();
        // XXX broken!
        //zcmb->ringbuf->shrink_last(zcmb->buf, sz);
        mut.unlock();
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

        struct iovec sendbufs[3];
        sendbufs[0].iov_base = (char *)&hdr;
        sendbufs[0].iov_len = sizeof(hdr);
        sendbufs[1].iov_base = (char *)msg.channel;
        sendbufs[1].iov_len = channel_size + 1;
        sendbufs[2].iov_base = msg.buf;
        sendbufs[2].iov_len = msg.len;

        // transmit
        int packet_size = sizeof(hdr) + payload_size;
        ZCM_DEBUG("transmitting %zu byte [%s] payload (%d byte pkt)",
                  msg.len, msg.channel, packet_size);

        struct msghdr mhdr;
        mhdr.msg_name = (struct sockaddr*) &dest_addr;
        mhdr.msg_namelen = sizeof(dest_addr);
        mhdr.msg_iov = sendbufs;
        mhdr.msg_iovlen = 3;
        mhdr.msg_control = NULL;
        mhdr.msg_controllen = 0;
        mhdr.msg_flags = 0;

        int status = ::sendmsg(sendfd.fd, &mhdr, 0);
        msg_seqno++;

        if (status == packet_size) return 0;
        else return status;
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
        int firstfrag_datasize = fragment_size - (channel_size + 1);
        assert(firstfrag_datasize <= msg.len);

        struct iovec    first_sendbufs[3];
        first_sendbufs[0].iov_base = (char *) &hdr;
        first_sendbufs[0].iov_len = sizeof (hdr);
        first_sendbufs[1].iov_base = (char *) msg.channel;
        first_sendbufs[1].iov_len = channel_size + 1;
        first_sendbufs[2].iov_base = msg.buf;
        first_sendbufs[2].iov_len = firstfrag_datasize;

        int packet_size = sizeof(hdr) + (channel_size + 1) + firstfrag_datasize;
        fragment_offset += firstfrag_datasize;
//        int status = writev (zcm->sendfd, first_sendbufs, 3);
        struct msghdr mhdr;
        mhdr.msg_name = (struct sockaddr*) &dest_addr;
        mhdr.msg_namelen = sizeof(dest_addr);
        mhdr.msg_iov = first_sendbufs;
        mhdr.msg_iovlen = 3;
        mhdr.msg_control = NULL;
        mhdr.msg_controllen = 0;
        mhdr.msg_flags = 0;
        int status = ::sendmsg(sendfd.fd, &mhdr, 0);

        // transmit the rest of the fragments
        for (u16 frag_no = 1; packet_size == status && frag_no < nfragments; frag_no++) {
            hdr.fragment_offset = htonl(fragment_offset);
            hdr.fragment_no = htons(frag_no);

            int fraglen = std::min(fragment_size, (int)msg.len - (int)fragment_offset);

            struct iovec sendbufs[2];
            sendbufs[0].iov_base = (char *) &hdr;
            sendbufs[0].iov_len = sizeof (hdr);
            sendbufs[1].iov_base = (char *) (msg.buf + fragment_offset);
            sendbufs[1].iov_len = fraglen;

            mhdr.msg_iov = sendbufs;
            mhdr.msg_iovlen = 2;
            status = ::sendmsg(sendfd.fd, &mhdr, 0);

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
    static Buffer *buf = NULL;
    if (buf != NULL) {
        Buffer::destroy(buf, ringbuf);
        inbufs_empty->enqueue(buf);
    }

    buf = udp_read_packet();
    if (buf == nullptr)
        return ZCM_EAGAIN;

   msg->channel = buf->channel_name;
   msg->len = buf->data_size;
   msg->buf = buf->buf + buf->data_offset;

   return ZCM_EOK;
}

UDPM::~UDPM()
{
    ZCM_DEBUG("closing zcm context");

    if (frag_bufs) {
        delete frag_bufs;
        frag_bufs = NULL;
    }

    if (inbufs_empty) {
        inbufs_empty->freeQueue(ringbuf);
        inbufs_empty = NULL;
    }
    if (inbufs_filled) {
        inbufs_filled->freeQueue(ringbuf);
        inbufs_filled = NULL;
    }
    if (ringbuf) {
        delete ringbuf;
        ringbuf = NULL;
    }
}

UDPM::UDPM(const string& ip, u16 port, size_t recv_buf_size, u8 ttl)
    : params(ip, port, recv_buf_size, ttl)
{
}

bool UDPM::init()
{
    ZCM_DEBUG("Initializing ZCM UDPM context...");
    ZCM_DEBUG("Multicast %s:%d", inet_ntoa(params.addr), ntohs(params.port));

    // setup destination multicast address
    memset(&dest_addr, 0, sizeof(dest_addr));
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_addr = params.addr;
    dest_addr.sin_port = params.port;

    // XXX do something about this...
    // XXX this is probably not a good thing to do in a constructor...
    // test connectivity
//     SOCKET testfd = socket(AF_INET, SOCK_DGRAM, 0);
//     if (connect(testfd, (struct sockaddr*) &dest_addr, sizeof(dest_addr)) < 0) {
//         perror ("connect");
// //XXX add this back
// // #ifdef __linux__
// //         linux_check_routing_table(dest_addr.sin_addr);
// // #endif
//         return false;
//     }
//     zcm_close_socket(testfd);


    // create a transmit socket
    //
    sendfd = UDPMSocket::createSendSocket(params.addr, params.ttl);
    if (!sendfd.isOpen()) return false;

    // allocate the fragment buffer hashtable
    frag_bufs = new FragBufStore(MAX_FRAG_BUF_TOTAL_SIZE, MAX_NUM_FRAG_BUFS);
    ZCM_DEBUG("allocating resources for receiving messages");

    // allocate multicast socket
    recvfd = UDPMSocket::createRecvSocket(params.addr, params.port);
    if (!recvfd.isOpen()) return false;

    inbufs_empty = new BufQueue();
    inbufs_filled = new BufQueue();
    ringbuf = new Ringbuffer(ZCM_RINGBUF_SIZE);

    for (size_t i = 0; i < ZCM_DEFAULT_RECV_BUFS; i++) {
        /* We don't set the receive buffer's data pointer yet because it
         * will be taken from the ringbuffer at receive time. */
        inbufs_empty->enqueue(new Buffer());
    }

    // XXX add back the self test
    // conduct a self-test just to make sure everything is working.
    // ZCM_DEBUG("ZCM: conducting self test");
    // int self_test_results = udpm_self_test(zcm);
    // g_static_rec_mutex_lock(&zcm->mutex);

    // if (0 == self_test_results) {
    //     ZCM_DEBUG("ZCM: self test successful");
    // } else {
    //     // self test failed.  destroy the read thread
    //     fprintf (stderr, "ZCM self test failed!!\n"
    //             "Check your routing tables and firewall settings\n");
    //     return false;
    // }
    // return self_test_results;

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
