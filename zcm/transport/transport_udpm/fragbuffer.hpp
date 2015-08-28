#ifndef _ZCM_TRANS_UDPM_FRAGBUFFER_HPP
#define _ZCM_TRANS_UDPM_FRAGBUFFER_HPP

#include "udpm.hpp"
#include "ringbuffer.hpp"

/************************* Packet Headers *******************/

struct ZCM2HeaderShort
{
    u32 magic;
    u32 msg_seqno;
};

struct ZCM2HeaderLong
{
    u32 magic;
    u32 msg_seqno;
    u32 msg_size;
    u32 fragment_offset;
    u16 fragment_no;
    u16 fragments_in_msg;
};

// if fragment_no == 0, then header is immediately followed by NULL-terminated
// ASCII-encoded channel name, followed by the payload data
// if fragment_no > 0, then header is immediately followed by the payload data


/************************* Utility Functions *******************/
static inline int
zcm_close_socket(SOCKET fd)
{
#ifdef WIN32
    return closesocket(fd);
#else
    return close(fd);
#endif
}

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


/******************** message buffer **********************/
typedef struct _zcm_buf {
    char  channel_name[ZCM_CHANNEL_MAXLEN+1];
    int   channel_size;      // length of channel name

    int64_t recv_utime;      // timestamp of first datagram receipt
    char *buf;               // pointer to beginning of message.  This includes
                             // the header for unfragmented messages, and does
                             // not include the header for fragmented messages.

    int   data_offset;       // offset to payload
    int   data_size;         // size of payload
    Ringbuffer *ringbuf;  // the ringbuffer used to allocate buf.  NULL if
                             // not allocated from ringbuf

    int   packet_size;       // total bytes received
    int   buf_size;          // bytes allocated

    struct sockaddr from;    // sender
    socklen_t fromlen;
    struct _zcm_buf *next;
} zcm_buf_t;



/******* Functions for managing a queue of message buffers *******/
struct BufQueue
{
    zcm_buf_t *head = nullptr;
    zcm_buf_t **tail = &head;
    int count = 0;

    BufQueue();

    zcm_buf_t *dequeue();
    void enqueue(zcm_buf_t *el);

    // NOTE: this should be the dtor
    void freeQueue(Ringbuffer *ringbuf);

    bool isEmpty();
};

// allocate a zcm_buf from the ringbuf. If there is no more space in the ringbuf
// it is replaced with a bigger one. In this case, the old ringbuffer will be
// cleaned up when zcm_buf_free_data() is called;
zcm_buf_t *zcm_buf_allocate_data(BufQueue *inbufs_empty, Ringbuffer **ringbuf);
void zcm_buf_free_data(zcm_buf_t *zcmb, Ringbuffer *ringbuf);

/******************** fragment buffer **********************/
struct FragBuf
{
    char   channel[ZCM_CHANNEL_MAXLEN+1];
    struct sockaddr_in from;
    char   *data;
    u32    data_size;
    u16    fragments_remaining;
    u32    msg_seqno;
    i64    last_packet_utime;

    FragBuf(struct sockaddr_in from, const char *channel, u32 msg_seqno,
            u32 data_size, u16 nfragments, i64 first_packet_utime);
    ~FragBuf();

    bool matchesSockaddr(struct sockaddr_in *addr);
};

/******************** fragment buffer store **********************/
struct FragBufStore
{
    u32 total_size = 0;
    u32 max_total_size;
    u32 max_frag_bufs;

    // TODO change this back to a hashtable, using the 'sockaddr_in' as the key
    //      like the original LCM code uses
    vector<FragBuf*> frag_bufs;

    FragBufStore(u32 max_total_size, u32 max_frag_bufs);
    ~FragBufStore();

    FragBuf *lookup(struct sockaddr_in *key);
    void add(FragBuf *fbuf);
    void remove(int index);
};


/************************* Linux Specific Functions *******************/
#ifdef __linux__
void linux_check_routing_table(struct in_addr zcm_mcaddr);
#endif


#endif  // _ZCM_TRANS_UDPM_UTIL_HPP
