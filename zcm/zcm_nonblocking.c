#include "zcm/zcm_nonblocking.h"
#include "zcm/util/debug.h"

#include <string.h>

#define MAX_SUBS 16
typedef struct sub sub_t;
struct sub
{
    char channel[32];
    zcm_callback_t *cb;
    void *usr;
};

struct zcm_nonblocking
{
    zcm_t *zcm;
    zcm_trans_nonblock_t *trans;

    // TODO speed this up
    size_t nsubs;
    sub_t subs[MAX_SUBS];
};

zcm_nonblocking_t *zcm_nonblocking_create(zcm_t *zcm, zcm_trans_nonblock_t *trans)
{
    zcm_nonblocking_t *z = calloc(1, sizeof(zcm_nonblocking_t));
    z->zcm = zcm;
    z->trans = trans;
    return z;
}

void zcm_nonblocking_destroy(zcm_nonblocking_t *z)
{
    if (z) {
        if (z->trans) zcm_trans_nonblock_destroy(z->trans);
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
    return zcm_trans_nonblock_sendmsg(z->trans, msg);
}

int zcm_nonblocking_subscribe(zcm_nonblocking_t *z, const char *channel, zcm_callback_t *cb, void *usr)
{
    size_t i = z->nsubs;
    strcpy(z->subs[i].channel, channel);
    z->subs[i].cb = cb;
    z->subs[i].usr = usr;
    z->nsubs++;

    return -1;
}
