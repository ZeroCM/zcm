#ifndef _ZCM_TRANS_UDPM_FRAGBUFFER_HPP
#define _ZCM_TRANS_UDPM_FRAGBUFFER_HPP

#include "udpm.hpp"
#include "mempool.hpp"

/************************* Packet Headers *******************/

struct MsgHeaderShort
{
    u32 magic;
    u32 msg_seqno;
};

struct MsgHeaderLong
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
struct Message
{
    char  channel_name[ZCM_CHANNEL_MAXLEN+1];
    int   channel_size;      // length of channel name

    i64   recv_utime;        // timestamp of first datagram receipt

    MemPool *mempool;        // Allocator for this (NULL if none)
    char *buf;
    size_t bufsize;

    int   data_offset;       // offset to payload
    int   data_size;         // size of payload

    struct sockaddr from;    // sender
    socklen_t fromlen;

    Message() { memset(this, 0, sizeof(*this)); }
};

struct Packet
{
    char   *buf;
    size_t  bufsize;

    struct sockaddr from;
    socklen_t fromlen;
    i64    recv_utime;
    size_t recv_size;

    Packet() { memset(this, 0, sizeof(*this)); }
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

    bool matchesSockaddr(struct sockaddr_in *addr);
};

/******************** fragment buffer store **********************/
// struct FragBufStore
// {
//     u32 total_size = 0;
//     u32 max_total_size;
//     u32 max_frag_bufs;

//     // TODO change this back to a hashtable, using the 'sockaddr_in' as the key
//     //      like the original LCM code uses
//     vector<FragBuf*> frag_bufs;

//     FragBufStore(u32 max_total_size, u32 max_frag_bufs);
//     ~FragBufStore();

//     FragBuf *lookup(struct sockaddr_in *key);
//     void add(FragBuf *fbuf);
//     FragBuf *remove(int index);
//     FragBuf *remove(FragBuf *fbuf);
// };

/************** A pool to handle every alloc/dealloc operation on Message objects ******/
struct MessagePool
{
    MessagePool(size_t maxSize, size_t maxBuffers);
    ~MessagePool();

    // Message
    Message *allocMessage();
    void freeMessageBuffer(Message *b);
    void freeMessage(Message *b);

    // FragBuf
    FragBuf *allocFragBuf(struct sockaddr_in from, const char *channel, u32 msg_seqno,
                          u32 data_size, u16 nfragments, i64 first_packet_utime);
    void freeFragBuf(FragBuf *f);

    FragBuf *lookupFragBuf(struct sockaddr_in *key);
    void addFragBuf(FragBuf *fbuf);
    FragBuf *removeFragBuf(int index);
    FragBuf *removeFragBuf(FragBuf *fbuf);


  private:
    MemPool mempool;
    vector<FragBuf*> fragbufs;
    size_t maxSize;
    size_t maxBuffers;
    size_t totalSize = 0;
};

#endif  // _ZCM_TRANS_UDPM_FRAGBUFFER_HPP
