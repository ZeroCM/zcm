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

/******************** message buffer **********************/
struct BufQueue;
struct Buffer
{
    char  channel_name[ZCM_CHANNEL_MAXLEN+1];
    int   channel_size;      // length of channel name

    i64   recv_utime;      // timestamp of first datagram receipt
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
    Buffer *next;

    // allocate a zcm_buf from the ringbuf. If there is no more space in the ringbuf
    // it is replaced with a bigger one. In this case, the old ringbuffer will be
    // cleaned up when zcm_buf_free_data() is called;
    static Buffer *make(BufQueue *inbufs_empty, Ringbuffer **ringbuf);
    static void destroy(Buffer *self, Ringbuffer *ringbuf);
};



/******* Functions for managing a queue of message buffers *******/
struct BufQueue
{
    Buffer *head = nullptr;
    Buffer **tail = &head;
    int count = 0;

    BufQueue();

    Buffer *dequeue();
    void enqueue(Buffer *el);

    // NOTE: this should be the dtor
    void freeQueue(Ringbuffer *ringbuf);

    bool isEmpty();
};

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

#endif  // _ZCM_TRANS_UDPM_FRAGBUFFER_HPP
