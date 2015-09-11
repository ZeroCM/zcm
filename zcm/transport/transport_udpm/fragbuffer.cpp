#include "fragbuffer.hpp"

static bool sockaddrEqual(struct sockaddr_in *a, struct sockaddr_in *b)
{
    return a->sin_addr.s_addr == b->sin_addr.s_addr &&
           a->sin_port        == b->sin_port &&
           a->sin_family      == b->sin_family;
}

bool FragBuf::matchesSockaddr(struct sockaddr_in *addr)
{
    return sockaddrEqual(&from, addr);
}

/************************* Utility Functions *******************/
// XXX DISABLED due to GLIB removal
/* static inline int */
/* zcm_timeval_compare (const GTimeVal *a, const GTimeVal *b) { */
/*     if (a->tv_sec == b->tv_sec && a->tv_usec == b->tv_usec) return 0; */
/*     if (a->tv_sec > b->tv_sec || */
/*             (a->tv_sec == b->tv_sec && a->tv_usec > b->tv_usec)) */
/*         return 1; */
/*     return -1; */
/* } */

/* static inline void */
/* zcm_timeval_add (const GTimeVal *a, const GTimeVal *b, GTimeVal *dest) */
/* { */
/*     dest->tv_sec = a->tv_sec + b->tv_sec; */
/*     dest->tv_usec = a->tv_usec + b->tv_usec; */
/*     if (dest->tv_usec > 999999) { */
/*         dest->tv_usec -= 1000000; */
/*         dest->tv_sec++; */
/*     } */
/* } */

/* static inline void */
/* zcm_timeval_subtract (const GTimeVal *a, const GTimeVal *b, GTimeVal *dest) */
/* { */
/*     dest->tv_sec = a->tv_sec - b->tv_sec; */
/*     dest->tv_usec = a->tv_usec - b->tv_usec; */
/*     if (dest->tv_usec < 0) { */
/*         dest->tv_usec += 1000000; */
/*         dest->tv_sec--; */
/*     } */
/* } */

/* static inline int64_t */
/* zcm_timestamp_now() */
/* { */
/*     GTimeVal tv; */
/*     g_get_current_time(&tv); */
/*     return (int64_t) tv.tv_sec * 1000000 + tv.tv_usec; */
/* } */

MessagePool::MessagePool(size_t maxSize, size_t maxBuffers)
    : maxSize(maxSize), maxBuffers(maxBuffers)
{
}

MessagePool::~MessagePool()
{
}

Message *MessagePool::allocMessage()
{
    Message *zcmb = new (mempool.alloc(sizeof(Message))) Message{};
    assert(zcmb);

    // allocate space on the ringbuffer for the packet data.
    // give it the maximum possible size for an unfragmented packet
    zcmb->mempool = &mempool;
    zcmb->buf = mempool.alloc(ZCM_MAX_UNFRAGMENTED_PACKET_SIZE);
    zcmb->bufsize = ZCM_MAX_UNFRAGMENTED_PACKET_SIZE;
    assert(zcmb->buf);

    return zcmb;
}

void MessagePool::freeMessageBuffer(Message *b)
{
    if (!b->buf)
        return;

    // XXX get rid of this! We should only be using 'mempool'
    if (b->mempool)
        b->mempool->free(b->buf, b->bufsize);
    else
        std::free(b->buf);

    b->mempool = NULL;
    b->buf = NULL;
    b->bufsize = 0;
}

void MessagePool::freeMessage(Message *b)
{
    freeMessageBuffer(b);
    mempool.free((char*)b, sizeof(*b));
}


FragBuf *MessagePool::allocFragBuf(struct sockaddr_in from, const char *channel, u32 msg_seqno,
                                   u32 data_size, u16 nfragments, i64 first_packet_utime)
{
    FragBuf *fbuf = (FragBuf*) mempool.alloc(sizeof(FragBuf));
    strncpy(fbuf->channel, channel, ZCM_CHANNEL_MAXLEN);
    fbuf->channel[ZCM_CHANNEL_MAXLEN] = '\0';
    fbuf->from = from;
    fbuf->msg_seqno = msg_seqno;
    fbuf->data = mempool.alloc(data_size);
    fbuf->data_size = data_size;
    fbuf->fragments_remaining = nfragments;
    fbuf->last_packet_utime = first_packet_utime;
    return fbuf;
}

void MessagePool::freeFragBuf(FragBuf *fbuf)
{
    if (fbuf->data)
        mempool.free(fbuf->data, fbuf->data_size);
    mempool.free((char*)fbuf, sizeof(FragBuf));
}

FragBuf *MessagePool::lookupFragBuf(struct sockaddr_in *key)
{
    for (auto& elt : fragbufs)
        if (elt->matchesSockaddr(key))
            return elt;
    return nullptr;
}

void MessagePool::addFragBuf(FragBuf *fbuf)
{
    while (totalSize > maxSize || fragbufs.size() > maxBuffers) {
        // find and remove the least recently updated fragment buffer
        int idx = -1;
        FragBuf *eldest = nullptr;
        for (size_t i = 0; i < fragbufs.size(); i++) {
            FragBuf *f = fragbufs[i];
            if (idx == -1 || f->last_packet_utime < eldest->last_packet_utime) {
                idx = (int)i;
                eldest = f;
            }
        }
        if (eldest) {
            removeFragBuf(idx);
            // XXX Need to free the removed FargBuf*
        }
    }

    fragbufs.push_back(fbuf);
    totalSize += fbuf->data_size;
}

FragBuf *MessagePool::removeFragBuf(int index)
{
    assert(0 <= index && index < (int)fragbufs.size());

    // Update the total_size of the fragment buffers
    FragBuf *fbuf = fragbufs[index];
    totalSize -= fbuf->data_size;

    // delete old element, move last element to this slot, and shrink by 1
    size_t lastIdx = fragbufs.size()-1;
    fragbufs[index] = fragbufs[lastIdx];
    fragbufs.resize(lastIdx);
    return fbuf;
}

FragBuf *MessagePool::removeFragBuf(FragBuf *fbuf)
{
    // NOTE: this is kinda slow...
    // Search for the fragbuf index
    for (int idx = 0; idx < (int)fragbufs.size(); idx++)
        if (fragbufs[idx] == fbuf)
            return this->removeFragBuf(idx);

    // Did not find
    assert(0 && "Tried to remove invalid fragbuf");
}

/************************* Linux Specific Functions *******************/
// #ifdef __linux__
// void linux_check_routing_table(struct in_addr zcm_mcaddr);
// #endif


// #ifdef __linux__
// static inline int _parse_inaddr(const char *addr_str, struct in_addr *addr)
// {
//     char buf[] = {
//         '0', 'x',
//         addr_str[6], addr_str[7],
//         addr_str[4], addr_str[5],
//         addr_str[2], addr_str[3],
//         addr_str[0], addr_str[1],
//         0
//     };
//     return inet_aton(buf, addr);
// }

// void
// linux_check_routing_table(struct in_addr zcm_mcaddr)
// {
//     FILE *fp = fopen("/proc/net/route", "r");
//     if(!fp) {
//         perror("Unable to open routing table (fopen)");
//         goto show_route_cmds;
//     }

//     // read and ignore the first line of the routing table file
//     char buf[1024];
//     if(!fgets(buf, sizeof(buf), fp)) {
//         perror("Unable to read routing table (fgets)");
//         fclose(fp);
//         goto show_route_cmds;
//     }

//     // each line is a routing table entry
//     while(!feof(fp)) {
//         memset(buf, 0, sizeof(buf));
//         if(!fgets(buf, sizeof(buf)-1, fp))
//             break;
//         gchar **words = g_strsplit(buf, "\t", 0);

//         // each line should have 11 words
//         int nwords;
//         for(nwords=0; words[nwords] != NULL; nwords++);
//         if(nwords != 11) {
//             g_strfreev(words);
//             fclose(fp);
//             fprintf(stderr, "Unable to parse routing table!  Strange format.");
//             goto show_route_cmds;
//         }

//         // destination is 2nd word, netmask is 8th word
//         struct in_addr dest, mask;
//         if(!_parse_inaddr(words[1], &dest) || !_parse_inaddr(words[7], &mask)) {
//             fprintf(stderr, "Unable to parse routing table!");
//             g_strfreev(words);
//             fclose(fp);
//             goto show_route_cmds;
//         }
//         g_strfreev(words);

// //        fprintf(stderr, "checking route (%s/%X)\n", inet_ntoa(dest),
// //                ntohl(mask.s_addr));

//         // does this routing table entry match the ZCM URL?
//         if((zcm_mcaddr.s_addr & mask.s_addr) == (dest.s_addr & mask.s_addr)) {
//             // yes, so there is a valid multicast route
//             fclose(fp);
//             return;
//         }
//     }
//     fclose(fp);

// show_route_cmds:
//     // if we get here, then none of the routing table entries matched the
//     // ZCM destination URL.
//     fprintf(stderr,
// "\nNo route to %s\n\n"
// "ZCM requires a valid multicast route.  If this is a Linux computer and is\n"
// "simply not connected to a network, the following commands are usually\n"
// "sufficient as a temporary solution:\n"
// "\n"
// "   sudo ifconfig lo multicast\n"
// "   sudo route add -net 224.0.0.0 netmask 240.0.0.0 dev lo\n"
// "\n"
// "For more information, visit:\n"
// "   http://zcm-proj.github.io/multicast_setup.html\n\n",
// inet_ntoa(zcm_mcaddr));
// }
// #endif
