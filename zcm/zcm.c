#include "zcm/zcm.h"
#include "zcm/blocking.h"
#include "zcm/nonblocking.h"
#include "zcm/util/debug.h"
#include <assert.h>

zcm_t *zcm_create(const char *url)
{
    zcm_t *z = malloc(sizeof(zcm_t));
    if (!zcm_init(z, url)) {
        free(z);
        return NULL;
    }
    return z;
}

void zcm_destroy(zcm_t *zcm)
{
    zcm_cleanup(zcm);
    free(zcm);
}

int zcm_init(zcm_t *zcm, const char *url)
{
    int ret = 0;
    zcm_url_t *u = zcm_url_create(url);
    const char *protocol = zcm_url_protocol(u);

    zcm_trans_create_func *creator = zcm_transport_find(protocol);
    if (creator) {
        zcm_trans_t *trans = creator(u);
        if (trans) {
            switch (trans->trans_type) {
                case ZCM_TRANS_BLOCK: {
                    zcm->type = ZCM_BLOCKING;
                    zcm->impl = zcm_blocking_create(zcm, trans);
                    ret = 1;
                } break;
                case ZCM_TRANS_NONBLOCK: {
                    zcm->type = ZCM_NONBLOCKING;
                    zcm->impl = zcm_nonblocking_create(zcm, trans);
                    ret = 1;
                } break;
            }
        } else {
            ZCM_DEBUG("failed to create transport for '%s'", url);
        }
    } else {
        ZCM_DEBUG("failed to find transport for '%s'", url);
    }

    zcm_url_destroy(u);
    return ret;
}

void zcm_cleanup(zcm_t *zcm)
{
    if (zcm) {
        switch (zcm->type) {
            case ZCM_BLOCKING:    zcm_blocking_destroy   (zcm->impl); break;
            case ZCM_NONBLOCKING: zcm_nonblocking_destroy(zcm->impl); break;
        }
    }
}

int zcm_publish(zcm_t *zcm, const char *channel, char *data, size_t len)
{
    switch (zcm->type) {
        case ZCM_BLOCKING:    return zcm_blocking_publish   (zcm->impl, channel, data, len); break;
        case ZCM_NONBLOCKING: return zcm_nonblocking_publish(zcm->impl, channel, data, len); break;
    }
    assert(0 && "unreachable");
}

int zcm_subscribe(zcm_t *zcm, const char *channel, zcm_callback_t *cb, void *usr)
{
    switch (zcm->type) {
        case ZCM_BLOCKING:    return zcm_blocking_subscribe   (zcm->impl, channel, cb, usr); break;
        case ZCM_NONBLOCKING: return zcm_nonblocking_subscribe(zcm->impl, channel, cb, usr); break;
    }
    assert(0 && "unreachable");
}

void zcm_become(zcm_t *zcm)
{
    switch (zcm->type) {
        case ZCM_BLOCKING:    return zcm_blocking_become(zcm->impl); break;
        case ZCM_NONBLOCKING: assert(0 && "Cannot become() on a nonblocking ZCM interface"); break;
    }
}

int zcm_handle_nonblock(zcm_t *zcm)
{
    switch (zcm->type) {
        case ZCM_BLOCKING:    assert(0 && "Cannot handle_nonblock() on a blocking ZCM interface"); break;
        case ZCM_NONBLOCKING: return zcm_nonblocking_handle_nonblock(zcm->impl); break;
    }
    assert(0 && "unreachable");
}
