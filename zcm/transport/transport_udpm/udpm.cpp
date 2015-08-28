#include "udpm.hpp"
#include "fragbuffer.hpp"

static inline int
zcm_close_socket(SOCKET fd)
{
#ifdef WIN32
    return closesocket(fd);
#else
    return close(fd);
#endif
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
};

struct UDPM
{
    SOCKET recvfd;
    SOCKET sendfd;
    struct sockaddr_in dest_addr;

    zcm_t *zcm;
    Params params;

    /* size of the kernel UDP receive buffer */
    size_t kernel_rbuf_sz;
    bool warned_about_small_kernel_buf;

    /* Packet structures available for sending or receiving use are
     * stored in the *_empty queues. */
    BufQueue *inbufs_empty;
    /* Received packets that are filled with data are queued here. */
    BufQueue *inbufs_filled;

    /* Memory for received small packets is taken from a fixed-size ring buffer
     * so we don't have to do any mallocs */
    Ringbuffer *ringbuf;

    std::mutex mut; /* Must be locked when reading/writing to the
                       above three queues */
    bool thread_created;
    std::thread read_thread;
    int notify_pipe[2];         // pipe to notify application when messages arrive
    int thread_msg_pipe[2];     // pipe to notify read thread when to quit

    std::mutex transmit_lock; // so that only thread at a time can transmit

    /* synchronization variables used only while allocating receive resources
     */
    bool creating_read_thread;
    std::condition_variable create_read_thread_cond;
    std::mutex create_read_thread_mutex;


    /* other variables */
    FragBufStore *frag_bufs;

    u32          udp_rx;            // packets received and processed
    u32          udp_discarded_bad; // packets discarded because they were bad
                                    // somehow
    double       udp_low_watermark; // least buffer available
    i32          udp_last_report_secs;

    u32          msg_seqno; // rolling counter of how many messages transmitted

    /***** Methods ******/
    UDPM(){}
    ~UDPM();
    void _destroy_recv_parts();
};


//static int _setup_recv_parts (UDPM *u);

// static GStaticPrivate CREATE_READ_THREAD_PKEY = G_STATIC_PRIVATE_INIT;

void UDPM::_destroy_recv_parts()
{
    if (thread_created) {
        // send the read thread an exit command
        int wstatus = zcm_internal_pipe_write(thread_msg_pipe[1], "\0", 1);
        if (wstatus < 0) {
            perror(__FILE__ " write(destroy)");
        } else {
            read_thread.join();
        }
        thread_created = false;
    }

    if (thread_msg_pipe[0] >= 0) {
        zcm_internal_pipe_close(thread_msg_pipe[0]);
        zcm_internal_pipe_close(thread_msg_pipe[1]);
        thread_msg_pipe[0] = thread_msg_pipe[1] = -1;
    }

    if (recvfd >= 0) {
        zcm_close_socket(recvfd);
        recvfd = -1;
    }

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

UDPM::~UDPM()
{
    // XXX add back debug
    //dbg (DBG_ZCM, "closing zcm context\n");
    _destroy_recv_parts();

    if (sendfd >= 0)
        zcm_close_socket(sendfd);

    zcm_internal_pipe_close(notify_pipe[0]);
    zcm_internal_pipe_close(notify_pipe[1]);
}

// static int parse_mc_addr_and_port (const char *str, Params *params)
// {
//     if (!str || !strlen (str)) {
//         str = "239.255.76.67:7667";
//     }

//     char **words = g_strsplit (str, ":", 2);
//     if (inet_aton (words[0], (struct in_addr*) &params->mc_addr) == 0) {
//         fprintf (stderr, "Error: Bad multicast IP address \"%s\"\n", words[0]);
//         goto fail;
//     }
//     if (words[1]) {
//         char *st = NULL;
//         int port = strtol (words[1], &st, 0);
//         if (st == words[1] || port < 0 || port > 65535) {
//             fprintf (stderr, "Error: Bad multicast port \"%s\"\n", words[1]);
//             goto fail;
//         }
//         params->mc_port = htons (port);
//     }
//     g_strfreev (words);
//     return 0;
// fail:
//     g_strfreev (words);
//     return -1;
// }

// static void
// new_argument (gpointer key, gpointer value, gpointer user)
// {
//     udpm_params_t * params = (udpm_params_t *) user;
//     if (!strcmp ((char *) key, "recv_buf_size")) {
//         char *endptr = NULL;
//         params->recv_buf_size = strtol ((char *) value, &endptr, 0);
//         if (endptr == value)
//             fprintf (stderr, "Warning: Invalid value for recv_buf_size\n");
//     }
//     else if (!strcmp ((char *) key, "ttl")) {
//         char *endptr = NULL;
//         params->mc_ttl = strtol ((char *) value, &endptr, 0);
//         if (endptr == value)
//             fprintf (stderr, "Warning: Invalid value for ttl\n");
//     }
//     else if (!strcmp ((char *) key, "transmit_only")) {
//         fprintf (stderr, "%s:%d -- transmit_only option is now obsolete\n",
//                 __FILE__, __LINE__);
//     }
//     else {
//         fprintf(stderr, "%s:%d -- unknown provider argument %s\n",
//                 __FILE__, __LINE__, (char *)key);
//     }
// }

// static int
// _recv_message_fragment (zcm_udpm_t *zcm, zcm_buf_t *zcmb, uint32_t sz)
// {
//     zcm2_header_long_t *hdr = (zcm2_header_long_t*) zcmb->buf;

//     // any existing fragment buffer for this message source?
//     zcm_frag_buf_t *fbuf = zcm_frag_buf_store_lookup(zcm->frag_bufs,
//             &zcmb->from);

//     uint32_t msg_seqno = ntohl (hdr->msg_seqno);
//     uint32_t data_size = ntohl (hdr->msg_size);
//     uint32_t fragment_offset = ntohl (hdr->fragment_offset);
// //    uint16_t fragment_no = ntohs (hdr->fragment_no);
//     uint16_t fragments_in_msg = ntohs (hdr->fragments_in_msg);
//     uint32_t frag_size = sz - sizeof (zcm2_header_long_t);
//     char *data_start = (char*) (hdr + 1);

//     // discard any stale fragments from previous messages
//     if (fbuf && ((fbuf->msg_seqno != msg_seqno) ||
//                  (fbuf->data_size != data_size))) {
//         zcm_frag_buf_store_remove (zcm->frag_bufs, fbuf);
//         dbg(DBG_ZCM, "Dropping message (missing %d fragments)\n",
//             fbuf->fragments_remaining);
//         fbuf = NULL;
//     }

// //    printf ("fragment %d/%d (offset %d/%d) seq %d packet sz: %d %p\n",
// //        ntohs(hdr->fragment_no) + 1, fragments_in_msg,
// //        fragment_offset, data_size, msg_seqno, sz, fbuf);

//     if (data_size > ZCM_MAX_MESSAGE_SIZE) {
//         dbg (DBG_ZCM, "rejecting huge message (%d bytes)\n", data_size);
//         return 0;
//     }

//     // create a new fragment buffer if necessary
//     if (!fbuf && hdr->fragment_no == 0) {
//         char *channel = (char*) (hdr + 1);
//         int channel_sz = strlen (channel);
//         if (channel_sz > ZCM_MAX_CHANNEL_NAME_LENGTH) {
//             dbg (DBG_ZCM, "bad channel name length\n");
//             zcm->udp_discarded_bad++;
//             return 0;
//         }

//         // if the packet has no subscribers, drop the message now.
//         if(!zcm_has_handlers(zcm->zcm, channel))
//             return 0;

//         fbuf = zcm_frag_buf_new (*((struct sockaddr_in*) &zcmb->from),
//                 channel, msg_seqno, data_size, fragments_in_msg,
//                 zcmb->recv_utime);
//         zcm_frag_buf_store_add (zcm->frag_bufs, fbuf);
//         data_start += channel_sz + 1;
//         frag_size -= (channel_sz + 1);
//     }

//     if (!fbuf) return 0;

// #ifdef __linux__
//     if(zcm->kernel_rbuf_sz < 262145 &&
//        data_size > zcm->kernel_rbuf_sz &&
//        ! zcm->warned_about_small_kernel_buf) {
//         fprintf(stderr,
// "==== ZCM Warning ===\n"
// "ZCM detected that large packets are being received, but the kernel UDP\n"
// "receive buffer is very small.  The possibility of dropping packets due to\n"
// "insufficient buffer space is very high.\n"
// "\n"
// "For more information, visit:\n"
// "   http://zcm-proj.github.io/multicast_setup.html\n\n");
//         zcm->warned_about_small_kernel_buf = 1;
//     }
// #endif

//     if (fragment_offset + frag_size > fbuf->data_size) {
//         dbg (DBG_ZCM, "dropping invalid fragment (off: %d, %d / %d)\n",
//                 fragment_offset, frag_size, fbuf->data_size);
//         zcm_frag_buf_store_remove (zcm->frag_bufs, fbuf);
//         return 0;
//     }

//     // copy data
//     memcpy (fbuf->data + fragment_offset, data_start, frag_size);
//     fbuf->last_packet_utime = zcmb->recv_utime;

//     fbuf->fragments_remaining --;

//     if (0 == fbuf->fragments_remaining) {
//         // complete message received.  Is there a subscriber that still
//         // wants it?  (i.e., does any subscriber have space in its queue?)
//         if(!zcm_try_enqueue_message(zcm->zcm, fbuf->channel)) {
//             // no... sad... free the fragment buffer and return
//             zcm_frag_buf_store_remove (zcm->frag_bufs, fbuf);
//             return 0;
//         }

//         // yes, transfer the message into the zcm_buf_t

//         // deallocate the ringbuffer-allocated buffer
//         g_static_rec_mutex_lock (&zcm->mutex);
//         zcm_buf_free_data(zcmb, zcm->ringbuf);
//         g_static_rec_mutex_unlock (&zcm->mutex);

//         // transfer ownership of the message's payload buffer
//         zcmb->buf = fbuf->data;
//         fbuf->data = NULL;

//         strcpy (zcmb->channel_name, fbuf->channel);
//         zcmb->channel_size = strlen (zcmb->channel_name);
//         zcmb->data_offset = 0;
//         zcmb->data_size = fbuf->data_size;
//         zcmb->recv_utime = fbuf->last_packet_utime;

//         // don't need the fragment buffer anymore
//         zcm_frag_buf_store_remove (zcm->frag_bufs, fbuf);

//         return 1;
//     }

//     return 0;
// }

// static int
// _recv_short_message (zcm_udpm_t *zcm, zcm_buf_t *zcmb, int sz)
// {
//     zcm2_header_short_t *hdr2 = (zcm2_header_short_t*) zcmb->buf;

//     // shouldn't have to worry about buffer overflow here because we
//     // zeroed out byte #65536, which is never written to by recv
//     const char *pkt_channel_str = (char*) (hdr2 + 1);

//     zcmb->channel_size = strlen (pkt_channel_str);

//     if (zcmb->channel_size > ZCM_MAX_CHANNEL_NAME_LENGTH) {
//         dbg (DBG_ZCM, "bad channel name length\n");
//         zcm->udp_discarded_bad++;
//         return 0;
//     }

//     zcm->udp_rx++;

//     // if the packet has no subscribers, drop the message now.
//     if(!zcm_try_enqueue_message(zcm->zcm, pkt_channel_str))
//         return 0;

//     strcpy (zcmb->channel_name, pkt_channel_str);

//     zcmb->data_offset =
//         sizeof (zcm2_header_short_t) + zcmb->channel_size + 1;

//     zcmb->data_size = sz - zcmb->data_offset;
//     return 1;
// }

// // read continuously until a complete message arrives
// static zcm_buf_t *
// udp_read_packet (zcm_udpm_t *zcm)
// {
//     zcm_buf_t *zcmb = NULL;

//     int sz = 0;

    // TODO warn about message loss somewhere else.

//    g_static_rec_mutex_lock (&zcm->mutex);
//    unsigned int ring_capacity = zcm_ringbuf_capacity(zcm->ringbuf);
//    unsigned int ring_used = zcm_ringbuf_used(zcm->ringbuf);
//    double buf_avail = ((double)(ring_capacity - ring_used)) / ring_capacity;
//    g_static_rec_mutex_unlock (&zcm->mutex);
//    if (buf_avail < zcm->udp_low_watermark)
//        zcm->udp_low_watermark = buf_avail;
//
//    GTimeVal tv;
//    g_get_current_time(&tv);
//    int elapsedsecs = tv.tv_sec - zcm->udp_last_report_secs;
//    if (elapsedsecs > 2) {
//        uint32_t total_bad = zcm->udp_discarded_bad;
//        if (total_bad > 0 || zcm->udp_low_watermark < 0.5) {
//            fprintf(stderr,
//                    "%d.%03d ZCM loss %4.1f%% : %5d err, "
//                    "buf avail %4.1f%%\n",
//                   (int) tv.tv_sec, (int) tv.tv_usec/1000,
//                   total_bad * 100.0 / (zcm->udp_rx + total_bad),
//                   zcm->udp_discarded_bad,
//                   100.0 * zcm->udp_low_watermark);
//
//            zcm->udp_rx = 0;
//            zcm->udp_discarded_bad = 0;
//            zcm->udp_last_report_secs = tv.tv_sec;
//            zcm->udp_low_watermark = HUGE;
//        }
//    }

//     int got_complete_message = 0;

//     while (!got_complete_message) {
//         // wait for either incoming UDP data, or for an abort message
//         fd_set fds;
//         FD_ZERO (&fds);
//         FD_SET (zcm->recvfd, &fds);
//         FD_SET (zcm->thread_msg_pipe[0], &fds);
//         SOCKET maxfd = MAX(zcm->recvfd, zcm->thread_msg_pipe[0]);

//         if (select (maxfd + 1, &fds, NULL, NULL, NULL) <= 0) {
//             perror ("udp_read_packet -- select:");
//             continue;
//         }

//         if (FD_ISSET (zcm->thread_msg_pipe[0], &fds)) {
//             // received an exit command.
//             dbg (DBG_ZCM, "read thread received exit command\n");
//             if (zcmb) {
//                 // zcmb is not on one of the memory managed buffer queues.  We could
//                 // either put it back on one of the queues, or just free it here.  Do the
//                 // latter.
//                 //
//                 // Can also just free its zcm_buf_t here.  Its data buffer is
//                 // managed either by the ring buffer or the fragment buffer, so
//                 // we can ignore it.
//                 free (zcmb);
//             }
//             return NULL;
//         }

//         // there is incoming UDP data ready.
//         assert (FD_ISSET (zcm->recvfd, &fds));

//         if (!zcmb) {
//             g_static_rec_mutex_lock (&zcm->mutex);
//             zcmb = zcm_buf_allocate_data(zcm->inbufs_empty, &zcm->ringbuf);
//             g_static_rec_mutex_unlock (&zcm->mutex);
//         }
//         struct iovec        vec;
//         vec.iov_base = zcmb->buf;
//         vec.iov_len = 65535;

//         struct msghdr msg;
//         memset(&msg, 0, sizeof(struct msghdr));
//         msg.msg_name = &zcmb->from;
//         msg.msg_namelen = sizeof (struct sockaddr);
//         msg.msg_iov = &vec;
//         msg.msg_iovlen = 1;
// #ifdef MSG_EXT_HDR
//         // operating systems that provide SO_TIMESTAMP allow us to obtain more
//         // accurate timestamps by having the kernel produce timestamps as soon
//         // as packets are received.
//         char controlbuf[64];
//         msg.msg_control = controlbuf;
//         msg.msg_controllen = sizeof (controlbuf);
//         msg.msg_flags = 0;
// #endif
//         sz = recvmsg (zcm->recvfd, &msg, 0);

//         if (sz < 0) {
//             perror ("udp_read_packet -- recvmsg");
//             zcm->udp_discarded_bad++;
//             continue;
//         }

//         if (sz < sizeof(zcm2_header_short_t)) {
//             // packet too short to be ZCM
//             zcm->udp_discarded_bad++;
//             continue;
//         }

//         zcmb->fromlen = msg.msg_namelen;

//         int got_utime = 0;
// #ifdef SO_TIMESTAMP
//         struct cmsghdr * cmsg = CMSG_FIRSTHDR (&msg);
//         /* Get the receive timestamp out of the packet headers if possible */
//         while (!zcmb->recv_utime && cmsg) {
//             if (cmsg->cmsg_level == SOL_SOCKET &&
//                     cmsg->cmsg_type == SCM_TIMESTAMP) {
//                 struct timeval * t = (struct timeval*) CMSG_DATA (cmsg);
//                 zcmb->recv_utime = (int64_t) t->tv_sec * 1000000 + t->tv_usec;
//                 got_utime = 1;
//                 break;
//             }
//             cmsg = CMSG_NXTHDR (&msg, cmsg);
//         }
// #endif
//         if (!got_utime)
//             zcmb->recv_utime = zcm_timestamp_now ();

//         zcm2_header_short_t *hdr2 = (zcm2_header_short_t*) zcmb->buf;
//         uint32_t rcvd_magic = ntohl(hdr2->magic);
//         if (rcvd_magic == ZCM2_MAGIC_SHORT)
//             got_complete_message = _recv_short_message (zcm, zcmb, sz);
//         else if (rcvd_magic == ZCM2_MAGIC_LONG)
//             got_complete_message = _recv_message_fragment (zcm, zcmb, sz);
//         else {
//             dbg (DBG_ZCM, "ZCM: bad magic\n");
//             zcm->udp_discarded_bad++;
//             continue;
//         }
//     }

//     // if the newly received packet is a short packet, then resize the space
//     // allocated to it on the ringbuffer to exactly match the amount of space
//     // required.  That way, we do not use 64k of the ringbuffer for every
//     // incoming message.
//     if (zcmb->ringbuf) {
//         g_static_rec_mutex_lock (&zcm->mutex);
//         zcm_ringbuf_shrink_last(zcmb->ringbuf, zcmb->buf, sz);
//         g_static_rec_mutex_unlock (&zcm->mutex);
//     }

//     return zcmb;
// }

// /* This is the receiver thread that runs continuously to retrieve any incoming
//  * ZCM packets from the network and queues them locally. */
// static void *
// recv_thread (void * user)
// {
// #ifdef G_OS_UNIX
//     // Mask out all signals on this thread.
//     sigset_t mask;
//     sigfillset(&mask);
//     pthread_sigmask(SIG_SETMASK, &mask, NULL);
// #endif

//     zcm_udpm_t * zcm = (zcm_udpm_t *) user;

//     while (1) {

//         zcm_buf_t *zcmb = udp_read_packet(zcm);
//         if (!zcmb) break;

//         /* If necessary, notify the reading thread by writing to a pipe.  We
//          * only want one character in the pipe at a time to avoid blocking
//          * writes, so we only do this when the queue transitions from empty to
//          * non-empty. */
//         g_static_rec_mutex_lock (&zcm->mutex);

//         if (zcm_buf_queue_is_empty (zcm->inbufs_filled))
//             if (zcm_internal_pipe_write(zcm->notify_pipe[1], "+", 1) < 0)
//                 perror ("write to notify");

//         /* Queue the packet for future retrieval by zcm_handle (). */
//         zcm_buf_enqueue (zcm->inbufs_filled, zcmb);

//         g_static_rec_mutex_unlock (&zcm->mutex);
//     }
//     dbg (DBG_ZCM, "read thread exiting\n");
//     return NULL;
// }

// static int
// zcm_udpm_get_fileno (zcm_udpm_t *zcm)
// {
//     if (_setup_recv_parts (zcm) < 0) {
//         return -1;
//     }
//     return zcm->notify_pipe[0];
// }

// static int
// zcm_udpm_subscribe (zcm_udpm_t *zcm, const char *channel)
// {
//     return _setup_recv_parts (zcm);
// }

// static int
// zcm_udpm_publish (zcm_udpm_t *zcm, const char *channel, const void *data,
//         unsigned int datalen)
// {
//     int channel_size = strlen (channel);
//     if (channel_size > ZCM_MAX_CHANNEL_NAME_LENGTH) {
//         fprintf (stderr, "ZCM Error: channel name too long [%s]\n",
//                 channel);
//         return -1;
//     }

//     int payload_size = channel_size + 1 + datalen;
//     if (payload_size <= ZCM_SHORT_MESSAGE_MAX_SIZE) {
//         // message is short.  send in a single packet

//         g_static_mutex_lock (&zcm->transmit_lock);

//         zcm2_header_short_t hdr;
//         hdr.magic = htonl (ZCM2_MAGIC_SHORT);
//         hdr.msg_seqno = htonl(zcm->msg_seqno);

//         struct iovec sendbufs[3];
//         sendbufs[0].iov_base = (char *) &hdr;
//         sendbufs[0].iov_len = sizeof (hdr);
//         sendbufs[1].iov_base = (char *) channel;
//         sendbufs[1].iov_len = channel_size + 1;
//         sendbufs[2].iov_base = (char *) data;
//         sendbufs[2].iov_len = datalen;

//         // transmit
//         int packet_size = datalen + sizeof (hdr) + channel_size + 1;
//         dbg (DBG_ZCM_MSG, "transmitting %d byte [%s] payload (%d byte pkt)\n",
//                 datalen, channel, packet_size);

// //        int status = writev (zcm->sendfd, sendbufs, 3);
//         struct msghdr msg;
//         msg.msg_name = (struct sockaddr*) &zcm->dest_addr;
//         msg.msg_namelen = sizeof(zcm->dest_addr);
//         msg.msg_iov = sendbufs;
//         msg.msg_iovlen = 3;
//         msg.msg_control = NULL;
//         msg.msg_controllen = 0;
//         msg.msg_flags = 0;
//         int status = sendmsg(zcm->sendfd, &msg, 0);

//         zcm->msg_seqno ++;
//         g_static_mutex_unlock (&zcm->transmit_lock);

//         if (status == packet_size) return 0;
//         else return status;
//     } else {
//         // message is large.  fragment into multiple packets

//         int fragment_size = ZCM_FRAGMENT_MAX_PAYLOAD;
//         int nfragments = payload_size / fragment_size +
//             !!(payload_size % fragment_size);

//         if (nfragments > 65535) {
//             fprintf (stderr, "ZCM error: too much data for a single message\n");
//             return -1;
//         }

//         // acquire transmit lock so that all fragments are transmitted
//         // together, and so that no other message uses the same sequence number
//         // (at least until the sequence # rolls over)
//         g_static_mutex_lock (&zcm->transmit_lock);
//         dbg (DBG_ZCM_MSG, "transmitting %d byte [%s] payload in %d fragments\n",
//                 payload_size, channel, nfragments);

//         uint32_t fragment_offset = 0;

//         zcm2_header_long_t hdr;
//         hdr.magic = htonl (ZCM2_MAGIC_LONG);
//         hdr.msg_seqno = htonl (zcm->msg_seqno);
//         hdr.msg_size = htonl (datalen);
//         hdr.fragment_offset = 0;
//         hdr.fragment_no = 0;
//         hdr.fragments_in_msg = htons (nfragments);

//         // first fragment is special.  insert channel before data
//         int firstfrag_datasize = fragment_size - (channel_size + 1);
//         assert (firstfrag_datasize <= datalen);

//         struct iovec    first_sendbufs[3];
//         first_sendbufs[0].iov_base = (char *) &hdr;
//         first_sendbufs[0].iov_len = sizeof (hdr);
//         first_sendbufs[1].iov_base = (char *) channel;
//         first_sendbufs[1].iov_len = channel_size + 1;
//         first_sendbufs[2].iov_base = (char *) data;
//         first_sendbufs[2].iov_len = firstfrag_datasize;

//         int packet_size = sizeof (hdr) + channel_size + 1 + firstfrag_datasize;
//         fragment_offset += firstfrag_datasize;
// //        int status = writev (zcm->sendfd, first_sendbufs, 3);
//         struct msghdr msg;
//         msg.msg_name = (struct sockaddr*) &zcm->dest_addr;
//         msg.msg_namelen = sizeof(zcm->dest_addr);
//         msg.msg_iov = first_sendbufs;
//         msg.msg_iovlen = 3;
//         msg.msg_control = NULL;
//         msg.msg_controllen = 0;
//         msg.msg_flags = 0;
//         int status = sendmsg(zcm->sendfd, &msg, 0);

//         // transmit the rest of the fragments
//         for (uint16_t frag_no=1;
//                 packet_size == status && frag_no<nfragments;
//                 frag_no++) {
//             hdr.fragment_offset = htonl (fragment_offset);
//             hdr.fragment_no = htons (frag_no);

//             int fraglen = MIN (fragment_size,
//                     datalen - fragment_offset);

//             struct iovec sendbufs[2];
//             sendbufs[0].iov_base = (char *) &hdr;
//             sendbufs[0].iov_len = sizeof (hdr);
//             sendbufs[1].iov_base = (char *) ((char *)data + fragment_offset);
//             sendbufs[1].iov_len = fraglen;

// //            status = writev (zcm->sendfd, sendbufs, 2);
//             msg.msg_iov = sendbufs;
//             msg.msg_iovlen = 2;
//             status = sendmsg(zcm->sendfd, &msg, 0);

//             fragment_offset += fraglen;
//             packet_size = sizeof (hdr) + fraglen;
//         }

//         // sanity check
//         if (0 == status) {
//             assert (fragment_offset == datalen);
//         }

//         zcm->msg_seqno ++;
//         g_static_mutex_unlock (&zcm->transmit_lock);
//     }

//     return 0;
// }

// static int
// zcm_udpm_handle (zcm_udpm_t *zcm)
// {
//     int status;
//     char ch;
//     if(0 != _setup_recv_parts (zcm))
//         return -1;

//     /* Read one byte from the notify pipe.  This will block if no packets are
//      * available yet and wake up when they are. */
//     status = zcm_internal_pipe_read(zcm->notify_pipe[0], &ch, 1);
//     if (status == 0) {
//         fprintf (stderr, "Error: zcm_handle read 0 bytes from notify_pipe\n");
//         return -1;
//     }
//     else if (status < 0) {
//         fprintf (stderr, "Error: zcm_handle read: %s\n", strerror (errno));
//         return -1;
//     }

//     /* Dequeue the next received packet */
//     g_static_rec_mutex_lock (&zcm->mutex);
//     zcm_buf_t * zcmb = zcm_buf_dequeue (zcm->inbufs_filled);

//     if (!zcmb) {
//         fprintf (stderr,
//                 "Error: no packet available despite getting notification.\n");
//         g_static_rec_mutex_unlock (&zcm->mutex);
//         return -1;
//     }

//     /* If there are still packets in the queue, put something back in the pipe
//      * so that future invocations will get called. */
//     if (!zcm_buf_queue_is_empty (zcm->inbufs_filled))
//         if (zcm_internal_pipe_write(zcm->notify_pipe[1], "+", 1) < 0)
//             perror ("write to notify");
//     g_static_rec_mutex_unlock (&zcm->mutex);

//     zcm_recv_buf_t rbuf;
//     rbuf.data = (uint8_t*) zcmb->buf + zcmb->data_offset;
//     rbuf.data_size = zcmb->data_size;
//     rbuf.recv_utime = zcmb->recv_utime;
//     rbuf.zcm = zcm->zcm;

//     if(zcm->creating_read_thread) {
//         // special case:  If we're creating the read thread and are in
//         // self-test mode, then only dispatch the self-test message.
//         if(!strcmp(zcmb->channel_name, SELF_TEST_CHANNEL))
//             zcm_dispatch_handlers (zcm->zcm, &rbuf, zcmb->channel_name);
//     } else {
//         zcm_dispatch_handlers (zcm->zcm, &rbuf, zcmb->channel_name);
//     }

//     g_static_rec_mutex_lock (&zcm->mutex);
//     zcm_buf_free_data(zcmb, zcm->ringbuf);
//     zcm_buf_enqueue (zcm->inbufs_empty, zcmb);
//     g_static_rec_mutex_unlock (&zcm->mutex);

//     return 0;
// }

// static void
// self_test_handler (const zcm_recv_buf_t *rbuf, const char *channel, void *user)
// {
//     int *result = (int*) user;
//     *result = 1;
// }

// static int
// udpm_self_test (zcm_udpm_t *zcm)
// {
//     int success = 0;
//     int status;
//     // register a handler for the self test message
//     zcm_subscription_t *h = zcm_subscribe (zcm->zcm, SELF_TEST_CHANNEL,
//                                            self_test_handler, &success);

//     // transmit a message
//     char *msg = "zcm self test";
//     zcm_udpm_publish (zcm, SELF_TEST_CHANNEL, (uint8_t*)msg, strlen (msg));

//     // wait one second for message to be received
//     GTimeVal now, endtime;
//     g_get_current_time(&now);
//     endtime.tv_sec = now.tv_sec + 10;
//     endtime.tv_usec = now.tv_usec;

//     // periodically retransmit, just in case
//     GTimeVal retransmit_interval = { 0, 100000 };
//     GTimeVal next_retransmit;
//     zcm_timeval_add (&now, &retransmit_interval, &next_retransmit);

//     int recvfd = zcm->notify_pipe[0];

//     do {
//         GTimeVal selectto;
//         zcm_timeval_subtract (&next_retransmit, &now, &selectto);

//         fd_set readfds;
//         FD_ZERO (&readfds);
//         FD_SET (recvfd,&readfds);

//         g_get_current_time(&now);
//         if (zcm_timeval_compare (&now, &next_retransmit) > 0) {
//             status = zcm_udpm_publish (zcm, SELF_TEST_CHANNEL, (uint8_t*)msg,
//                     strlen (msg));
//             zcm_timeval_add (&now, &retransmit_interval, &next_retransmit);
//         }

//         status=select (recvfd + 1,&readfds,0,0, (struct timeval*) &selectto);
//         if (status > 0 && FD_ISSET (recvfd,&readfds)) {
//             zcm_udpm_handle (zcm);
//         }
//         g_get_current_time(&now);

//     } while (! success && zcm_timeval_compare (&now, &endtime) < 0);

//     zcm_unsubscribe (zcm->zcm, h);

//     dbg (DBG_ZCM, "ZCM: self test complete\n");

//     // if the self test message was received, then the handler modified the
//     // value of success to be 1
//     return (success == 1)?0:-1;
// }

// static int
// _setup_recv_parts (zcm_udpm_t *zcm)
// {
//     g_static_rec_mutex_lock(&zcm->mutex);

//     // some thread synchronization code to ensure that only one thread sets up the
//     // receive thread, and that all threads entering this function after the thread
//     // setup begins wait for it to finish.
//     if(zcm->creating_read_thread) {
//         // check if this thread is the one creating the receive thread.
//         // If so, just return.
//         if(g_static_private_get(&CREATE_READ_THREAD_PKEY)) {
//             g_static_rec_mutex_unlock(&zcm->mutex);
//             return 0;
//         }

//         // ugly bit with two mutexes because we can't use a GStaticRecMutex with a GCond
//         g_mutex_lock(zcm->create_read_thread_mutex);
//         g_static_rec_mutex_unlock(&zcm->mutex);

//         // wait for the thread creating the read thread to finish
//         while(zcm->creating_read_thread) {
//             g_cond_wait(zcm->create_read_thread_cond, zcm->create_read_thread_mutex);
//         }
//         g_mutex_unlock(zcm->create_read_thread_mutex);
//         g_static_rec_mutex_lock(&zcm->mutex);

//         // if we've gotten here, then either the read thread is created, or it
//         // was not possible to do so.  Figure out which happened, and return.
//         int result = zcm->thread_created ? 0 : -1;
//         g_static_rec_mutex_unlock(&zcm->mutex);
//         return result;
//     } else if(zcm->thread_created) {
//         g_static_rec_mutex_unlock(&zcm->mutex);
//         return 0;
//     }

//     // no other thread is trying to create the read thread right now.  claim that task.
//     zcm->creating_read_thread = 1;
//     zcm->create_read_thread_mutex = g_mutex_new();
//     zcm->create_read_thread_cond = g_cond_new();
//     // mark this thread as the one creating the read thread
//     g_static_private_set(&CREATE_READ_THREAD_PKEY, GINT_TO_POINTER(1), NULL);

//     dbg (DBG_ZCM, "allocating resources for receiving messages\n");

//     // allocate the fragment buffer hashtable
//     zcm->frag_bufs = zcm_frag_buf_store_new(MAX_FRAG_BUF_TOTAL_SIZE,
//             MAX_NUM_FRAG_BUFS);

//     // allocate multicast socket
//     zcm->recvfd = socket (AF_INET, SOCK_DGRAM, 0);
//     if (zcm->recvfd < 0) {
//         perror ("allocating ZCM recv socket");
//         goto setup_recv_thread_fail;
//     }

//     struct sockaddr_in addr;
//     memset (&addr, 0, sizeof (addr));
//     addr.sin_family = AF_INET;
//     addr.sin_addr.s_addr = INADDR_ANY;
//     addr.sin_port = zcm->params.mc_port;

//     // allow other applications on the local machine to also bind to this
//     // multicast address and port
//     int opt=1;
//     dbg (DBG_ZCM, "ZCM: setting SO_REUSEADDR\n");
//     if (setsockopt (zcm->recvfd, SOL_SOCKET, SO_REUSEADDR,
//             (char*)&opt, sizeof (opt)) < 0) {
//         perror ("setsockopt (SOL_SOCKET, SO_REUSEADDR)");
//         goto setup_recv_thread_fail;
//     }

// #ifdef USE_REUSEPORT
//     /* Mac OS and FreeBSD require the REUSEPORT option in addition
//      * to REUSEADDR or it won't let multiple processes bind to the
//      * same port, even if they are using multicast. */
//     dbg (DBG_ZCM, "ZCM: setting SO_REUSEPORT\n");
//     if (setsockopt (zcm->recvfd, SOL_SOCKET, SO_REUSEPORT,
//             (char*)&opt, sizeof (opt)) < 0) {
//         perror ("setsockopt (SOL_SOCKET, SO_REUSEPORT)");
//         goto setup_recv_thread_fail;
//     }
// #endif

// #if 0
//     // set loopback option so that packets sent out on the multicast socket
//     // are also delivered to it
//     unsigned char lo_opt = 1;
//     dbg (DBG_ZCM, "ZCM: setting multicast loopback option\n");
//     status = setsockopt (zcm->recvfd, IPPROTO_IP, IP_MULTICAST_LOOP,
//             &lo_opt, sizeof (lo_opt));
//     if (status < 0) {
//         perror ("setting multicast loopback");
//         return -1;
//     }
// #endif

// #ifdef WIN32
//     // Windows has small (8k) buffer by default
//     // Increase it to a default reasonable amount
//     int recv_buf_size = 2048 * 1024;
//     setsockopt(zcm->recvfd, SOL_SOCKET, SO_RCVBUF,
//             (char*)&recv_buf_size, sizeof(recv_buf_size));
// #endif

//     // debugging... how big is the receive buffer?
//     unsigned int retsize = sizeof (int);
//     getsockopt (zcm->recvfd, SOL_SOCKET, SO_RCVBUF,
//             (char*)&zcm->kernel_rbuf_sz, (socklen_t *) &retsize);
//     dbg (DBG_ZCM, "ZCM: receive buffer is %d bytes\n", zcm->kernel_rbuf_sz);
//     if (zcm->params.recv_buf_size) {
//         if (setsockopt (zcm->recvfd, SOL_SOCKET, SO_RCVBUF,
//                 (char *) &zcm->params.recv_buf_size,
//                 sizeof (zcm->params.recv_buf_size)) < 0) {
//             perror ("setsockopt(SOL_SOCKET, SO_RCVBUF)");
//             fprintf (stderr, "Warning: Unable to set recv buffer size\n");
//         }
//         getsockopt (zcm->recvfd, SOL_SOCKET, SO_RCVBUF,
//                 (char*)&zcm->kernel_rbuf_sz, (socklen_t *) &retsize);
//         dbg (DBG_ZCM, "ZCM: receive buffer is %d bytes\n", zcm->kernel_rbuf_sz);

//         if (zcm->params.recv_buf_size > zcm->kernel_rbuf_sz) {
//             g_warning ("ZCM UDP receive buffer size (%d) \n"
//                     "       is smaller than reqested (%d). "
//                     "For more info:\n"
//                     "       http://zcm-proj.github.io/multicast_setup.html\n",
//                     zcm->kernel_rbuf_sz, zcm->params.recv_buf_size);
//         }
//     }

//     /* Enable per-packet timestamping by the kernel, if available */
// #ifdef SO_TIMESTAMP
//     opt = 1;
//     setsockopt (zcm->recvfd, SOL_SOCKET, SO_TIMESTAMP, &opt, sizeof (opt));
// #endif

//     if (bind (zcm->recvfd, (struct sockaddr*)&addr, sizeof (addr)) < 0) {
//         perror ("bind");
//         goto setup_recv_thread_fail;
//     }

//     struct ip_mreq mreq;
//     mreq.imr_multiaddr = zcm->params.mc_addr;
//     mreq.imr_interface.s_addr = INADDR_ANY;
//     // join the multicast group
//     dbg (DBG_ZCM, "ZCM: joining multicast group\n");
//     if (setsockopt (zcm->recvfd, IPPROTO_IP, IP_ADD_MEMBERSHIP,
//             (char*)&mreq, sizeof (mreq)) < 0) {
//         perror ("setsockopt (IPPROTO_IP, IP_ADD_MEMBERSHIP)");
//         goto setup_recv_thread_fail;
//     }

//     zcm->inbufs_empty = zcm_buf_queue_new ();
//     zcm->inbufs_filled = zcm_buf_queue_new ();
//     zcm->ringbuf = zcm_ringbuf_new (ZCM_RINGBUF_SIZE);

//     int i;
//     for (i = 0; i < ZCM_DEFAULT_RECV_BUFS; i++) {
//         /* We don't set the receive buffer's data pointer yet because it
//          * will be taken from the ringbuffer at receive time. */
//         zcm_buf_t * zcmb = (zcm_buf_t *) calloc (1, sizeof (zcm_buf_t));
//         zcm_buf_enqueue (zcm->inbufs_empty, zcmb);
//     }

//     // setup a pipe for notifying the reader thread when to quit
//     if(0 != zcm_internal_pipe_create(zcm->thread_msg_pipe)) {
//         perror(__FILE__ " pipe(setup)");
//         goto setup_recv_thread_fail;
//     }
//     fcntl (zcm->thread_msg_pipe[1], F_SETFL, O_NONBLOCK);

//     /* Start the reader thread */
//     zcm->read_thread = g_thread_create (recv_thread, zcm, TRUE, NULL);
//     if (!zcm->read_thread) {
//         fprintf (stderr, "Error: ZCM failed to start reader thread\n");
//         goto setup_recv_thread_fail;
//     }
//     zcm->thread_created = 1;
//     g_static_rec_mutex_unlock(&zcm->mutex);

//     // conduct a self-test just to make sure everything is working.
//     dbg (DBG_ZCM, "ZCM: conducting self test\n");
//     int self_test_results = udpm_self_test(zcm);
//     g_static_rec_mutex_lock(&zcm->mutex);

//     if (0 == self_test_results) {
//         dbg (DBG_ZCM, "ZCM: self test successful\n");
//     } else {
//         // self test failed.  destroy the read thread
//         fprintf (stderr, "ZCM self test failed!!\n"
//                 "Check your routing tables and firewall settings\n");
//         _destroy_recv_parts (zcm);
//     }

//     // notify threads waiting for the read thread to be created
//     g_mutex_lock(zcm->create_read_thread_mutex);
//     zcm->creating_read_thread = 0;
//     g_cond_broadcast(zcm->create_read_thread_cond);
//     g_mutex_unlock(zcm->create_read_thread_mutex);
//     g_static_rec_mutex_unlock(&zcm->mutex);

//     return self_test_results;

// setup_recv_thread_fail:
//     _destroy_recv_parts (zcm);
//     g_static_rec_mutex_unlock(&zcm->mutex);
//     return -1;
// }

// zcm_provider_t *
// zcm_udpm_create (zcm_t * parent, const char *network, const GHashTable *args)
// {
//     udpm_params_t params;
//     memset (&params, 0, sizeof (udpm_params_t));

//     g_hash_table_foreach ((GHashTable*) args, new_argument, &params);

//     if (parse_mc_addr_and_port (network, &params) < 0) {
//         return NULL;
//     }

//     zcm_udpm_t * zcm = (zcm_udpm_t *) calloc (1, sizeof (zcm_udpm_t));

//     zcm->zcm = parent;
//     zcm->params = params;
//     zcm->recvfd = -1;
//     zcm->sendfd = -1;
//     zcm->thread_msg_pipe[0] = zcm->thread_msg_pipe[1] = -1;
//     zcm->udp_low_watermark = 1.0;

//     zcm->kernel_rbuf_sz = 0;
//     zcm->warned_about_small_kernel_buf = 0;

//     zcm->frag_bufs = NULL;

//     // synchronization variables used when allocating receive resources
//     zcm->creating_read_thread = 0;
//     zcm->create_read_thread_mutex = NULL;
//     zcm->create_read_thread_cond = NULL;

//     // internal notification pipe
//     if(0 != zcm_internal_pipe_create(zcm->notify_pipe)) {
//         perror(__FILE__ " pipe(create)");
//         zcm_udpm_destroy (zcm);
//         return NULL;
//     }
//     fcntl (zcm->notify_pipe[1], F_SETFL, O_NONBLOCK);

//     g_static_rec_mutex_init (&zcm->mutex);
//     g_static_mutex_init (&zcm->transmit_lock);

//     dbg (DBG_ZCM, "Initializing ZCM UDPM context...\n");
//     dbg (DBG_ZCM, "Multicast %s:%d\n", inet_ntoa(params.mc_addr), ntohs (params.mc_port));

//     // setup destination multicast address
//     memset (&zcm->dest_addr, 0, sizeof (zcm->dest_addr));
//     zcm->dest_addr.sin_family = AF_INET;
//     zcm->dest_addr.sin_addr = params.mc_addr;
//     zcm->dest_addr.sin_port = params.mc_port;

//     // test connectivity
//     SOCKET testfd = socket (AF_INET, SOCK_DGRAM, 0);
//     if (connect (testfd, (struct sockaddr*) &zcm->dest_addr,
//                 sizeof (zcm->dest_addr)) < 0) {

//         perror ("connect");
//         zcm_udpm_destroy (zcm);
// #ifdef __linux__
//         linux_check_routing_table(zcm->dest_addr.sin_addr);
// #endif
//         return NULL;
//     }
//     zcm_close_socket(testfd);

//     // create a transmit socket
//     //
//     // don't use connect() on the actual transmit socket, because linux then
//     // has problems multicasting to localhost
//     zcm->sendfd = socket (AF_INET, SOCK_DGRAM, 0);

//     // set multicast TTL
//     if (params.mc_ttl == 0) {
//         dbg (DBG_ZCM, "ZCM multicast TTL set to 0.  Packets will not "
//                 "leave localhost\n");
//     }
//     dbg (DBG_ZCM, "ZCM: setting multicast packet TTL to %d\n", params.mc_ttl);
//     if (setsockopt (zcm->sendfd, IPPROTO_IP, IP_MULTICAST_TTL,
//                 (char *) &params.mc_ttl, sizeof (params.mc_ttl)) < 0) {
//         perror ("setsockopt(IPPROTO_IP, IP_MULTICAST_TTL)");
//         zcm_udpm_destroy (zcm);
//         return NULL;
//     }

// #ifdef WIN32
//     // Windows has small (8k) buffer by default
//     // increase the send buffer to a reasonable amount.
//     int send_buf_size = 256 * 1024;
//     setsockopt(zcm->sendfd, SOL_SOCKET, SO_SNDBUF,
//             (char*)&send_buf_size, sizeof(send_buf_size));
// #endif

//     // debugging... how big is the send buffer?
//     int sockbufsize = 0;
//     unsigned int retsize = sizeof(int);
//     getsockopt(zcm->sendfd, SOL_SOCKET, SO_SNDBUF,
//             (char*)&sockbufsize, (socklen_t *) &retsize);
//     dbg (DBG_ZCM, "ZCM: send buffer is %d bytes\n", sockbufsize);

//     // set loopback option on the send socket
// #ifdef __sun__
//     unsigned char send_lo_opt = 1;
// #else
//     unsigned int send_lo_opt = 1;
// #endif
//     if (setsockopt (zcm->sendfd, IPPROTO_IP, IP_MULTICAST_LOOP,
//                 (char *) &send_lo_opt, sizeof (send_lo_opt)) < 0) {
//         perror ("setsockopt (IPPROTO_IP, IP_MULTICAST_LOOP)");
//         zcm_udpm_destroy (zcm);
//         return NULL;
//     }

//     // don't start the receive thread yet.  Only allocate resources for
//     // receiving messages when a subscription is made.

//     // However, we still need to setup sendfd in multi-cast group
//     struct ip_mreq mreq;
//     mreq.imr_multiaddr = zcm->params.mc_addr;
//     mreq.imr_interface.s_addr = INADDR_ANY;
//     dbg (DBG_ZCM, "ZCM: joining multicast group\n");
//     if (setsockopt (zcm->sendfd, IPPROTO_IP, IP_ADD_MEMBERSHIP,
//             (char*)&mreq, sizeof (mreq)) < 0) {
// #ifdef WIN32
//       // ignore this error in windows... see issue #60
// #else
//         perror ("setsockopt (IPPROTO_IP, IP_ADD_MEMBERSHIP)");
//         zcm_udpm_destroy (zcm);
//         return NULL;
// #endif
//     }

//     return zcm;
// }

// static zcm_provider_vtable_t udpm_vtable;
// static zcm_provider_info_t udpm_info;

// void
// zcm_udpm_provider_init (GPtrArray * providers)
// {
// // Because of Microsoft Visual Studio compiler
// // difficulties, do this now, not statically
//     udpm_vtable.create      = zcm_udpm_create;
//     udpm_vtable.destroy     = zcm_udpm_destroy;
//     udpm_vtable.subscribe   = zcm_udpm_subscribe;
//     udpm_vtable.unsubscribe = NULL;
//     udpm_vtable.publish     = zcm_udpm_publish;
//     udpm_vtable.handle      = zcm_udpm_handle;
//     udpm_vtable.get_fileno  = zcm_udpm_get_fileno;

//     udpm_info.name = "udpm";
//     udpm_info.vtable = &udpm_vtable;

//     g_ptr_array_add (providers, &udpm_info);
// }
