#include "zcm/zcm_private.h"
#include "zcm/transport.h"
#include "zcm/nonblocking.h"

#include <string.h>

/* TODO remove malloc for preallocated mem and linked-lists */
#ifndef ZCM_NONBLOCK_SUBS_MAX
#define ZCM_NONBLOCK_SUBS_MAX 512
#endif

struct zcm_nonblocking
{
    zcm_t *z;
    zcm_trans_t *zt;

    bool allChannelsEnabled;

    /* TODO speed this up */
    zcm_sub_t subs[ZCM_NONBLOCK_SUBS_MAX];
    bool      subInUse[ZCM_NONBLOCK_SUBS_MAX];
    size_t    subInUseEnd;
};

static bool isRegexChannel(const char* c, size_t clen)
{
    /* These chars are considered regex */
    size_t i;
    for (i = 0; i < clen; ++i)
        if (c[i] == '(' || c[i] == ')' || c[i] == '|' ||
            c[i] == '.' || c[i] == '*' || c[i] == '+') return true;

    return false;
}

static bool isSupportedRegex(const char* c, size_t clen)
{
    /* Currently only support strings formed as such: */
    /* "[any non-regex character any number of times].*" */
    if (!isRegexChannel(c, clen)) return true;

    if (clen < 2) return false;
    if (c[clen - 1] != '*') return false;
    if (c[clen - 2] != '.') return false;

    size_t i;
    for (i = 0; i < clen - 2; ++i)
        if (!((c[i] >= 'a' && c[i] <= 'z') ||
              (c[i] >= 'A' && c[i] <= 'Z') ||
              (c[i] >= '0' && c[i] <= '9') ||
              (c[i] == '_'))) return false;

    return true;
}

zcm_nonblocking_t *zcm_nonblocking_create(zcm_t *z, zcm_trans_t *zt)
{
    zcm_nonblocking_t *zcm;

    zcm = malloc(sizeof(zcm_nonblocking_t));
    if (!zcm) return NULL;
    zcm->z = z;
    zcm->zt = zt;
    zcm->allChannelsEnabled = false;

    size_t i;
    for (i = 0; i < ZCM_NONBLOCK_SUBS_MAX; ++i)
        zcm->subInUse[i] = false;

    zcm->subInUseEnd = 0;
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

int zcm_nonblocking_publish(zcm_nonblocking_t *z, const char *channel,
                            const uint8_t *data, uint32_t len)
{
    zcm_msg_t msg;

    msg.channel = channel;
    msg.len = len;
    /* Casting away constness okay because msg isn't used past end of function */
    msg.buf = (uint8_t*) data;
    return zcm_trans_sendmsg(z->zt, msg);
}

zcm_sub_t *zcm_nonblocking_subscribe(zcm_nonblocking_t *zcm, const char *channel,
                                     zcm_msg_handler_t cb, void *usr)
{
    int rc;
    size_t i;

    size_t clen = strlen(channel);
    bool regex = isRegexChannel(channel, clen);
    if (regex) {
        if (!isSupportedRegex(channel, clen)) return NULL;
        if (!zcm->allChannelsEnabled) {
            rc = zcm_trans_recvmsg_enable(zcm->zt, NULL, true);
            zcm->allChannelsEnabled = true;
        } else {
            rc = ZCM_EOK;
        }
    } else {
        rc = zcm_trans_recvmsg_enable(zcm->zt, channel, true);
    }

    if (rc != ZCM_EOK) {
        return NULL;
    }

    for (i = 0; i <= zcm->subInUseEnd && i < ZCM_NONBLOCK_SUBS_MAX; ++i) {
        if (!zcm->subInUse[i]) {

            strncpy(zcm->subs[i].channel, channel,
                    sizeof(zcm->subs[i].channel)/sizeof(zcm->subs[i].channel[0]));
            zcm->subs[i].callback = cb;
            zcm->subs[i].usr = usr;
            zcm->subInUse[i] = true;

            if (i == zcm->subInUseEnd) ++zcm->subInUseEnd;

            return &zcm->subs[i];
        }
    }
    return NULL;
}

int zcm_nonblocking_unsubscribe(zcm_nonblocking_t *zcm, zcm_sub_t *sub)
{
    size_t i;
    int    match_idx = sub - zcm->subs;
    size_t num_chan_matches = 0;
    int rc = ZCM_EOK;
    for (i = 0; i < zcm->subInUseEnd; ++i) {
        /* Note: it would be nice if we didn't have to do a string comp to unsubscribe, but
                 we need to count the number of channel matches so we know when we can disable
                 the transport's recvmsg_enable */
        if (strncmp(sub->channel, zcm->subs[i].channel,
                    sizeof(zcm->subs[i].channel)/sizeof(zcm->subs[i].channel[0])) == 0) {
            ++num_chan_matches;
        }
    }

    if (0 <= match_idx && match_idx < zcm->subInUseEnd) {
        if (num_chan_matches <= 1) {
            rc = zcm_trans_recvmsg_enable(zcm->zt, sub->channel, false);
        }

        zcm->subInUse[match_idx] = false;
        while (zcm->subInUseEnd > 0 && !zcm->subInUse[zcm->subInUseEnd - 1]) {
            --zcm->subInUseEnd;
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
    for (i = 0; i < zcm->subInUseEnd; ++i) {
        if (!zcm->subInUse[i]) continue;

        size_t subsChanLen = strlen(zcm->subs[i].channel);
        if (isRegexChannel(zcm->subs[i].channel, subsChanLen)) {
            size_t msgLen = strlen(msg->channel);
            /* This only works because isSupportedRegex() is called on subscribe */
            if (msgLen > 2 &&
                strncmp(zcm->subs[i].channel, msg->channel, subsChanLen - 2) == 0) {

                rbuf.zcm = zcm->z;
                rbuf.data = msg->buf;
                rbuf.data_size = msg->len;
                rbuf.recv_utime = msg->utime;

                sub = &zcm->subs[i];
                sub->callback(&rbuf, msg->channel, sub->usr);
            }
        } else {
            if (strcmp(zcm->subs[i].channel, msg->channel) == 0) {
                rbuf.zcm = zcm->z;
                rbuf.data = msg->buf;
                rbuf.data_size = msg->len;
                rbuf.recv_utime = msg->utime;

                sub = &zcm->subs[i];
                sub->callback(&rbuf, msg->channel, sub->usr);
            }
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
    if ((ret = zcm_trans_recvmsg(zcm->zt, &msg, 0)) != ZCM_EOK) return ret;
    dispatch_message(zcm, &msg);

    return ZCM_EOK;
}

void zcm_nonblocking_flush(zcm_nonblocking_t* zcm)
{
    /* Call twice because we need to make sure publish and subscribe are both handled */
    zcm_trans_update(zcm->zt);
    zcm_trans_update(zcm->zt);

    zcm_msg_t msg;
    while (zcm_trans_recvmsg(zcm->zt, &msg, 0) == ZCM_EOK)
        dispatch_message(zcm, &msg);
}
