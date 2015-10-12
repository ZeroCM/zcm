#include "zcm/transport.h"
#include "zcm/nonblocking.h"

#include <string.h>

/* TODO remove malloc for preallocated mem and linked-lists */
#define ZCM_NONBLOCK_SUBS_MAX 16

typedef struct sub sub_t;
struct sub
{
    char channel[ZCM_CHANNEL_MAXLEN+1];
    zcm_callback_t *cb;
    void *usr;
};

struct zcm_nonblocking
{
    zcm_t *zcm;
    zcm_trans_t *trans;

    /* TODO speed this up */
    size_t nsubs;
    sub_t subs[ZCM_NONBLOCK_SUBS_MAX];
};

zcm_nonblocking_t *zcm_nonblocking_create(zcm_t *zcm, zcm_trans_t *trans)
{
    zcm_nonblocking_t *z;

    z = malloc(sizeof(zcm_nonblocking_t));
    if (!z) return NULL;
    z->zcm = zcm;
    z->trans = trans;
    z->nsubs = 0;
    return z;
}

void zcm_nonblocking_destroy(zcm_nonblocking_t *z)
{
    if (z) {
        if (z->trans) zcm_trans_destroy(z->trans);
        free(z);
        z = NULL;
    }
}

int zcm_nonblocking_publish(zcm_nonblocking_t *z, const char *channel, char *data, size_t len)
{
    zcm_msg_t msg;

    msg.channel = channel;
    msg.len = len;
    msg.buf = data;
    return zcm_trans_sendmsg(z->trans, msg);
}

int zcm_nonblocking_subscribe(zcm_nonblocking_t *z, const char *channel, zcm_callback_t *cb, void *usr)
{
    int ret;
    size_t i;

    ret = zcm_trans_recvmsg_enable(z->trans, channel, true);
    if (ret != ZCM_EOK)
        return ret;

    i = z->nsubs;
    if (i >= ZCM_NONBLOCK_SUBS_MAX)
        return ZCM_EINVALID;
    strcpy(z->subs[i].channel, channel);
    z->subs[i].cb = cb;
    z->subs[i].usr = usr;
    z->nsubs++;

    return -1;
}

static void dispatch_message(zcm_nonblocking_t *z, zcm_msg_t *msg)
{
    zcm_recv_buf_t rbuf;
    sub_t *sub;
    size_t i;

    for (i = 0; i < z->nsubs; i++) {
        if (strcmp(z->subs[i].channel, msg->channel) == 0) {
            rbuf.zcm = z->zcm;
            rbuf.data = (char*)msg->buf;
            rbuf.data_size = msg->len;
            rbuf.recv_utime = 0;
            sub = &z->subs[i];
            sub->cb(&rbuf, msg->channel, sub->usr);
        }
    }
}

int zcm_nonblocking_handle_nonblock(zcm_nonblocking_t *z)
{
    int ret;
    zcm_msg_t msg;

    /* Perform any required traansport-level updates */
    zcm_trans_update(z->trans);

    /* Try to receive a messages from the transport and dispatch them */
    if ((ret=zcm_trans_recvmsg(z->trans, &msg, 0)) != ZCM_EOK)
        return ret;
    dispatch_message(z, &msg);

    return ZCM_EOK;
}
