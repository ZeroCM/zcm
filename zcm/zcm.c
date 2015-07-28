#include "zcm/zcm.h"
#include "zcm/zcm_blocking.h"
#include "zcm/zcm_nonblocking.h"
#include "zcm/util/debug.h"
#include <assert.h>

enum zcm_type { BLOCKING, NONBLOCKING };
struct zcm_t
{
    enum zcm_type type;
    union {
        zcm_blocking_t    *blocking;
        zcm_nonblocking_t *nonblocking;
    };
};

zcm_t *zcm_create(const char *url)
{
    zcm_url_t *u = zcm_url_create(url);
    const char *protocol = zcm_url_protocol(u);

    zcm_t *ret = NULL;
    zcm_transport_t *trans = zcm_transport_find(protocol);
    if (trans) {
        ret = malloc(sizeof(zcm_t));
        switch (trans->type) {
            case ZCM_TRANS_BLOCK: {
                zcm_trans_t *tr = trans->blocking(u);
                if (tr) {
                    ret->type = BLOCKING;
                    ret->blocking = zcm_blocking_create(ret, tr);
                } else {
                    ZCM_DEBUG("failed to create transport for '%s'", url);
                }
            } break;
            case ZCM_TRANS_NONBLOCK: {
                zcm_trans_nonblock_t *tr = trans->nonblocking(u);
                if (tr) {
                    ret->type = NONBLOCKING;
                    ret->nonblocking = zcm_nonblocking_create(ret, tr);
                } else {
                    ZCM_DEBUG("failed to create transport for '%s'", url);
                }
            } break;
        }
    } else {
        ZCM_DEBUG("failed to find transport for '%s'", url);
    }

    zcm_url_destroy(u);
    return ret;
}

void zcm_destroy(zcm_t *zcm)
{
    if (zcm) {
        switch (zcm->type) {
            case BLOCKING:    zcm_blocking_destroy   (zcm->blocking);    break;
            case NONBLOCKING: zcm_nonblocking_destroy(zcm->nonblocking); break;
        }
        free(zcm);
    }
}

int zcm_publish(zcm_t *zcm, const char *channel, char *data, size_t len)
{
    switch (zcm->type) {
        case BLOCKING:    return zcm_blocking_publish   (zcm->blocking,    channel, data, len); break;
        case NONBLOCKING: return zcm_nonblocking_publish(zcm->nonblocking, channel, data, len); break;
    }
    assert(0 && "unreachable");
}

int zcm_subscribe(zcm_t *zcm, const char *channel, zcm_callback_t *cb, void *usr)
{
    switch (zcm->type) {
        case BLOCKING:    return zcm_blocking_subscribe   (zcm->blocking,    channel, cb, usr); break;
        case NONBLOCKING: return zcm_nonblocking_subscribe(zcm->nonblocking, channel, cb, usr); break;
    }
    assert(0 && "unreachable");
}

void zcm_become(zcm_t *zcm)
{
    switch (zcm->type) {
        case BLOCKING:    return zcm_blocking_become(zcm->blocking); break;
        case NONBLOCKING: assert(0 && "Cannot become() on a nonblocking ZCM interface"); break;
    }
}
