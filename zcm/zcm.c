
/*******************************************************************************
 * ZCM Core API
 *
 *     This file implements the core ZCM API for both blocking and non-blocking modes
 *     The code is a bit hairy here because it must be C89 and be able to conditionally
 *     build only non-blocking. These requirements are needed to support embedded.
 *
 *******************************************************************************/

#include <assert.h>

#include "zcm/zcm.h"
#include "zcm/nonblocking.h"

#ifndef ZCM_EMBEDDED
# include "zcm/blocking.h"
# include "zcm/transport_registrar.h"
# include "zcm/util/debug.h"
#else
/** XXX here's to hoping that variadic macros are "portable enough" **/
# define ZCM_DEBUG(...)
#endif

zcm_t *zcm_create(const char *url)
{
#ifndef ZCM_EMBEDDED
    zcm_t *z = malloc(sizeof(zcm_t));
    if (!zcm_init(z, url)) {
        free(z);
        return NULL;
    }
    return z;
#else
    assert(0 && "zcm_create() with a url is not supported");
#endif
}

zcm_t *zcm_create_trans(zcm_trans_t *zt)
{
    zcm_t *z = malloc(sizeof(zcm_t));
    if (!zcm_init_trans(z, zt)) {
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
#ifndef ZCM_EMBEDDED
    int ret = 0;
    zcm_url_t *u = zcm_url_create(url);
    const char *protocol = zcm_url_protocol(u);

    zcm_trans_create_func *creator = zcm_transport_find(protocol);
    if (creator) {
        zcm_trans_t *trans = creator(u);
        if (trans) {
            ret = zcm_init_trans(zcm, trans);
        } else {
            ZCM_DEBUG("failed to create transport for '%s'", url);
        }
    } else {
        ZCM_DEBUG("failed to find transport for '%s'", url);
    }

    zcm_url_destroy(u);
    return ret;
#else
    assert(0 && "zcm_init() with a url is not supported");
#endif
}

int zcm_init_trans(zcm_t *zcm, zcm_trans_t *zt)
{
    switch (zt->trans_type) {
        case ZCM_BLOCKING: {
#ifndef ZCM_EMBEDDED
            zcm->type = ZCM_BLOCKING;
            zcm->impl = zcm_blocking_create(zcm, zt);
            return 1;
#else
            assert(0 && "the blocking api is not supported");
            return 0;
#endif
        } break;
        case ZCM_NONBLOCKING: {
            zcm->type = ZCM_NONBLOCKING;
            zcm->impl = zcm_nonblocking_create(zcm, zt);
            return 1;
        } break;
    }
    return 0;
}

void zcm_cleanup(zcm_t *zcm)
{
    if (zcm) {
#ifndef ZCM_EMBEDDED
        switch (zcm->type) {
            case ZCM_BLOCKING:    zcm_blocking_destroy   (zcm->impl); break;
            case ZCM_NONBLOCKING: zcm_nonblocking_destroy(zcm->impl); break;
        }
#else
        assert(zcm->type == ZCM_NONBLOCKING);
        zcm_nonblocking_destroy(zcm->impl);
#endif
    }
}

int zcm_publish(zcm_t *zcm, const char *channel, char *data, size_t len)
{
#ifndef ZCM_EMBEDDED
    switch (zcm->type) {
        case ZCM_BLOCKING:    return zcm_blocking_publish   (zcm->impl, channel, data, len); break;
        case ZCM_NONBLOCKING: return zcm_nonblocking_publish(zcm->impl, channel, data, len); break;
    }
#else
    assert(zcm->type == ZCM_NONBLOCKING);
    return zcm_nonblocking_publish(zcm->impl, channel, data, len);
#endif
    assert(0 && "unreachable");
}

int zcm_subscribe(zcm_t *zcm, const char *channel, zcm_callback_t *cb, void *usr)
{
#ifndef ZCM_EMBEDDED
    switch (zcm->type) {
        case ZCM_BLOCKING:    return zcm_blocking_subscribe   (zcm->impl, channel, cb, usr); break;
        case ZCM_NONBLOCKING: return zcm_nonblocking_subscribe(zcm->impl, channel, cb, usr); break;
    }
#else
    assert(zcm->type == ZCM_NONBLOCKING);
    return zcm_nonblocking_subscribe(zcm->impl, channel, cb, usr);
#endif
    assert(0 && "unreachable");
}

void zcm_become(zcm_t *zcm)
{
#ifndef ZCM_EMBEDDED
    switch (zcm->type) {
        case ZCM_BLOCKING:    return zcm_blocking_become(zcm->impl); break;
        case ZCM_NONBLOCKING: assert(0 && "Cannot become() on a nonblocking ZCM interface"); break;
    }
#else
    assert(0 && "the blocking api is not supported");
#endif
}

int zcm_handle_nonblock(zcm_t *zcm)
{
#ifndef ZCM_EMBEDDED
    switch (zcm->type) {
        case ZCM_BLOCKING:    assert(0 && "Cannot handle_nonblock() on a blocking ZCM interface"); break;
        case ZCM_NONBLOCKING: return zcm_nonblocking_handle_nonblock(zcm->impl); break;
    }
#else
    assert(zcm->type == ZCM_NONBLOCKING);
    return zcm_nonblocking_handle_nonblock(zcm->impl);
#endif
    assert(0 && "unreachable");
}
