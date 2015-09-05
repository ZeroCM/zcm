#include "fragbuffer.hpp"

/******************** fragment buffer **********************/
FragBuf::FragBuf(struct sockaddr_in from, const char *channel, u32 msg_seqno,
                 u32 data_size, u16 nfragments, i64 first_packet_utime)
{
    strncpy(this->channel, channel, ZCM_CHANNEL_MAXLEN);
    this->channel[ZCM_CHANNEL_MAXLEN] = '\0';
    this->from = from;
    this->msg_seqno = msg_seqno;
    this->data = (char*) malloc(data_size);
    this->data_size = data_size;
    this->fragments_remaining = nfragments;
    this->last_packet_utime = first_packet_utime;
}

FragBuf::~FragBuf()
{
    free(this->data);
}

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

FragBufStore::FragBufStore(u32 max_total_size, u32 max_frag_bufs) :
    max_total_size(max_total_size), max_frag_bufs(max_frag_bufs)
{
}

FragBufStore::~FragBufStore()
{
    // XXX need to cleanup the zcm_frag_buf_t* in the 'frag_bufs' vector
}

FragBuf *FragBufStore::lookup(struct sockaddr_in *key)
{
    for (auto& elt : frag_bufs)
        if (elt->matchesSockaddr(key))
            return elt;
    return nullptr;
}

void FragBufStore::add(FragBuf *fbuf)
{
    while (total_size > max_total_size || frag_bufs.size() > max_frag_bufs) {
        // find and remove the least recently updated fragment buffer
        int idx = -1;
        FragBuf *eldest = nullptr;
        for (int i = 0; i < (int)frag_bufs.size(); i++) {
            auto& f = frag_bufs[i];
            if (idx == -1 || f->last_packet_utime < eldest->last_packet_utime) {
                idx = i;
                eldest = frag_bufs[i];
            }
        }
        if (eldest)
            remove(idx);
    }

    frag_bufs.push_back(fbuf);
    total_size += fbuf->data_size;
}

void FragBufStore::remove(int index)
{
    assert(0 <= index && index < (int)frag_bufs.size());

    // Update the total_size of the fragment buffers
    FragBuf *fbuf = frag_bufs[index];
    total_size -= fbuf->data_size;

    // delete old element, move last element to this slot, and shrink by 1
    delete fbuf;
    size_t lastIdx = frag_bufs.size()-1;
    frag_bufs[index] = frag_bufs[lastIdx];
    frag_bufs.resize(lastIdx);
}

void FragBufStore::remove(FragBuf *fbuf)
{
    // NOTE: this is kinda slow...
    // Search for the fragbuf index
    for (int idx = 0; idx < (int)frag_bufs.size(); idx++) {
        if (frag_bufs[idx] == fbuf) {
            this->remove(idx);
            return;
        }
    }

    // Did not find
    assert(0 && "Tried to remove invalid fragbuf");
}

BufPool::BufPool()
{
    ringbuf = new Ringbuffer(ZCM_RINGBUF_SIZE);
}

BufPool::~BufPool()
{
    while (!freelist.empty()) {
        Buffer *b = freelist.top();
        freelist.pop();
        this->freeUnderlying(b);
    }

    if (ringbuf)
        delete ringbuf;
}

Buffer *BufPool::alloc()
{
    Buffer *zcmb = NULL;

    // first allocate a buffer struct for the packet metadata
    if (freelist.empty()) {
        // allocate additional buffer structs if needed
        for (size_t i = 0; i < ZCM_DEFAULT_RECV_BUFS; i++)
            freelist.push(new Buffer());
    }

    zcmb = freelist.top();
    freelist.pop();
    assert(zcmb);

    // allocate space on the ringbuffer for the packet data.
    // give it the maximum possible size for an unfragmented packet
    zcmb->buf = ringbuf->alloc(ZCM_MAX_UNFRAGMENTED_PACKET_SIZE);
    if (zcmb->buf == NULL) {
        // ringbuffer is full.  allocate a larger ringbuffer

        // Can't free the old ringbuffer yet because it's in use (i.e., full)
        // Must wait until later to free it.
        assert(ringbuf->get_used() > 0);
        ZCM_DEBUG("Orphaning ringbuffer %p\n", ringbuf);

        size_t old_capacity = ringbuf->get_capacity();
        size_t new_capacity = old_capacity + old_capacity/2; // 1.5 * old_capacity

        // replace the passed in ringbuf with the new one
        ringbuf = new Ringbuffer(new_capacity);
        zcmb->buf = ringbuf->alloc(ZCM_MAX_UNFRAGMENTED_PACKET_SIZE);
        assert(zcmb->buf);
        ZCM_DEBUG("Allocated new ringbuffer size %zu\n", new_capacity);
    }

    // save a pointer to the ringbuf, in case it gets replaced by another call
    zcmb->ringbuf = ringbuf;
    return zcmb;
}

void BufPool::freeUnderlying(Buffer *b)
{
    if(!b->buf)
        return;
    if (b->ringbuf) {
        b->ringbuf->dealloc(b->buf);

        // if the packet was allocated from an obsolete and empty ringbuffer,
        // then deallocate the old ringbuffer as well.
        if (b->ringbuf != ringbuf && !b->ringbuf->get_used()) {
            ZCM_DEBUG("Destroying unused orphan ringbuffer %p\n", b->ringbuf);
            delete b->ringbuf;
        }
    } else {
        std::free(b->buf);
    }
    b->buf = NULL;
    b->buf_size = 0;
    b->ringbuf = NULL;
}

void BufPool::free(Buffer *b)
{
    freeUnderlying(b);
    freelist.push(b);
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
