#include "zcm/zcm_private.h"
#include "zcm/transport.h"
#include "zcm/nonblocking.h"

#include <string.h>

/* TODO remove malloc for preallocated mem and linked-lists */
#define ZCM_NONBLOCK_SUBS_MAX 16

struct zcm_nonblocking
{
    zcm_t *z;
    zcm_trans_t *zt;

    /* TODO speed this up */
    size_t nsubs;
    zcm_sub_t subs[ZCM_NONBLOCK_SUBS_MAX];
};

zcm_nonblocking_t *zcm_nonblocking_create(zcm_t *z, zcm_trans_t *zt)
{
    zcm_nonblocking_t *zcm;

    zcm = malloc(sizeof(zcm_nonblocking_t));
    if (!zcm) return NULL;
    zcm->z = z;
    zcm->zt = zt;
    zcm->nsubs = 0;
    return zcm;
}

void zcm_nonblocking_destroy(zcm_nonblocking_t *zcm)
{
    if (zcm) {
        if (zcm->zt) zcm_trans_destroy(zcm->zt);
        free(zcm);
        zcm = NULL;
    }
}

int zcm_nonblocking_publish(zcm_nonblocking_t *z, const char *channel, const char *data,
                            uint32_t len)
{
    zcm_msg_t msg;

    msg.channel = channel;
    msg.len = len;
    msg.buf = (char*)data;
    return zcm_trans_sendmsg(z->zt, msg);
}

zcm_sub_t *zcm_nonblocking_subscribe(zcm_nonblocking_t *zcm, const char *channel,
                                     zcm_msg_handler_t cb, void *usr)
{
    int ret;
    size_t i;

    ret = zcm_trans_recvmsg_enable(zcm->zt, channel, true);
    if (ret != ZCM_EOK) {
        return NULL;
    }

    i = zcm->nsubs;
    if (i >= ZCM_NONBLOCK_SUBS_MAX) {
        return NULL;
    }
    strncpy(zcm->subs[i].channel, channel,
            sizeof(zcm->subs[i].channel)/sizeof(zcm->subs[i].channel[0]));
    zcm->subs[i].callback = cb;
    zcm->subs[i].usr = usr;
    zcm->nsubs++;

    return &zcm->subs[i];
}

int zcm_nonblocking_unsubscribe(zcm_nonblocking_t *zcm, zcm_sub_t *sub)
{
    size_t i;
    size_t match_idx = zcm->nsubs;
    size_t num_chan_matches = 0;
    int rc = ZCM_EOK;
    for (i = 0; i < zcm->nsubs; i++) {
        /* Note: it would be nice if we didn't have to do a string comp to unsubscribe, but
                 we need to count the number of channel matches so we know when we can disable
                 the transport's recvvmsg_enable */
        if (strncmp(sub->channel, zcm->subs[i].channel,
                    sizeof(zcm->subs[i].channel)/sizeof(zcm->subs[i].channel[0])) == 0) {
            ++num_chan_matches;
            if (sub == &zcm->subs[i]) {
                match_idx = i;
            }
        }
    }

    if (match_idx != zcm->nsubs) {
        if (num_chan_matches <= 1) {
            rc = zcm_trans_recvmsg_enable(zcm->zt, sub->channel, false);
        }

        --zcm->nsubs;
        if (match_idx < zcm->nsubs) {
            strncpy(zcm->subs[match_idx].channel, zcm->subs[zcm->nsubs].channel,
                    sizeof(zcm->subs[i].channel)/sizeof(zcm->subs[i].channel[0]));
            zcm->subs[match_idx].callback = zcm->subs[zcm->nsubs].callback;
            zcm->subs[match_idx].usr = zcm->subs[zcm->nsubs].usr;
        }
    } else {
        rc = ZCM_EINVALID;
    }

    return rc;
}

static void dispatch_message(zcm_nonblocking_t *zcm, zcm_msg_t *msg)
{
    zcm_recv_buf_t rbuf;
    zcm_sub_t *sub;
    size_t i;

    for (i = 0; i < zcm->nsubs; i++) {
        if (strcmp(zcm->subs[i].channel, msg->channel) == 0) {
            rbuf.zcm = zcm->z;
            rbuf.data = (char*)msg->buf;
            rbuf.data_size = msg->len;
            rbuf.recv_utime = msg->utime;

            sub = &zcm->subs[i];
            sub->callback(&rbuf, msg->channel, sub->usr);
        }
    }
}

int zcm_nonblocking_handle_nonblock(zcm_nonblocking_t *zcm)
{
    int ret;
    zcm_msg_t msg;

    /* Perform any required traansport-level updates */
    zcm_trans_update(zcm->zt);

    /* Try to receive a messages from the transport and dispatch them */
    if ((ret = zcm_trans_recvmsg(zcm->zt, &msg, 0)) != ZCM_EOK)
        return ret;
    dispatch_message(zcm, &msg);

    return ZCM_EOK;
}
