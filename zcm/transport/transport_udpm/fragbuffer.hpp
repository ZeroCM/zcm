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
    // Fields NOT set by the allocator object
    char  channel_name[ZCM_CHANNEL_MAXLEN+1];
    int   channel_size;      // length of channel name

    i64   recv_utime;        // timestamp of first datagram receipt

    int   data_offset;       // offset to payload
    int   data_size;         // size of payload

    struct sockaddr from;    // sender
    socklen_t fromlen;

    // Fields set by the allocator object
    char *buf;
    size_t bufsize;

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
    // Fields NOT set by the allocator object
    char   channel[ZCM_CHANNEL_MAXLEN+1];
    struct sockaddr_in from;
    u16    fragments_remaining;
    u32    msg_seqno;
    i64    last_packet_utime;

    // Fields set by the allocator object
    char   *data;
    u32    data_size;

    bool matchesSockaddr(struct sockaddr_in *addr);
};





/************** A pool to handle every alloc/dealloc operation on Message objects ******/
struct MessagePool
{
    MessagePool(size_t maxSize, size_t maxBuffers);
    ~MessagePool();

    // Message
    Message *allocMessage();
    void freeMessage(Message *b);

    // FragBuf
    FragBuf *addFragBuf(u32 data_size);
    FragBuf *lookupFragBuf(struct sockaddr_in *key);
    void removeFragBuf(FragBuf *fbuf);

    void transferBufffer(Message *to, FragBuf *from);

  private:
    void _freeMessageBuffer(Message *b);
    void _removeFragBuf(int index);

  private:
    MemPool mempool;
    vector<FragBuf*> fragbufs;
    size_t maxSize;
    size_t maxBuffers;
    size_t totalSize = 0;
};

#endif  // _ZCM_TRANS_UDPM_FRAGBUFFER_HPP
