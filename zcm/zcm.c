
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
#include "zcm/zcm_private.h"
#include "zcm/nonblocking.h"

#ifndef ZCM_EMBEDDED
#include <stdlib.h>

# include "zcm/blocking.h"
# include "zcm/transport_registrar.h"
# include "zcm/util/debug.h"
#else
/** Note: here's to hoping that variadic macros are "portable enough" **/
# define ZCM_DEBUG(...)
#endif

#ifndef ZCM_EMBEDDED
zcm_t* zcm_create(const char* url)
{
    zcm_t* z = malloc(sizeof(zcm_t));
    ZCM_ASSERT(z);
    if (zcm_init(z, url) == -1) {
        free(z);
        return NULL;
    }
    return z;
}
#endif

zcm_t* zcm_create_trans(zcm_trans_t* zt)
{
    zcm_t* z = malloc(sizeof(zcm_t));
    ZCM_ASSERT(z);
    if (zcm_init_trans(z, zt) == -1) {
        free(z);
        return NULL;
    }
    return z;
}

void zcm_destroy(zcm_t* zcm)
{
    zcm_cleanup(zcm);
    free(zcm);
}

#ifndef ZCM_EMBEDDED
int zcm_init(zcm_t* zcm, const char* url)
{
    zcm->err = ZCM_ECONNECT;
    /* If we have no url, try to use the env var */
    if (!url || url[0] == '\0') {
        url = getenv("ZCM_DEFAULT_URL");
        if (!url) {
            fprintf(stderr, "Please specify zcm url when creating zcm or "
                            "set environment variable ZCM_DEFAULT_URL\n");
            return -1;
        }
    }
    int ret = -1;
    zcm_url_t* u = zcm_url_create(url);
    ZCM_ASSERT(u);
    const char* protocol = zcm_url_protocol(u);

    zcm_trans_create_func* creator = zcm_transport_find(protocol);
    if (creator) {
        zcm_trans_t* trans = creator(u);
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
}
#endif

int zcm_init_trans(zcm_t* zcm, zcm_trans_t* zt)
{
    if (zt == NULL) goto fail;

#ifndef ZCM_EMBEDDED
    switch (zt->trans_type) {
        case ZCM_BLOCKING: {
            zcm->type = ZCM_BLOCKING;
            zcm->impl = zcm_blocking_create(zcm, zt);
            ZCM_ASSERT(zcm->impl);
            zcm->err = ZCM_EOK;
            return 0;
        }
        case ZCM_NONBLOCKING: {
            zcm->type = ZCM_NONBLOCKING;
            zcm->impl = zcm_nonblocking_create(zcm, zt);
            ZCM_ASSERT(zcm->impl);
            zcm->err = ZCM_EOK;
            return 0;
        }
    }
#endif
    assert(zt->trans_type == ZCM_NONBLOCKING);
    zcm->type = ZCM_NONBLOCKING;
    zcm->impl = zcm_nonblocking_create(zcm, zt);
    ZCM_ASSERT(zcm->impl);
    zcm->err = ZCM_EOK;
    return 0;

 fail:
    zcm->type = ZCM_NONBLOCKING;
    zcm->impl = NULL;
    zcm->err = ZCM_ECONNECT;
    return -1;
}

void zcm_cleanup(zcm_t* zcm)
{
    if (zcm) {
#ifndef ZCM_EMBEDDED
        switch (zcm->type) {
            case ZCM_BLOCKING:    zcm_blocking_destroy   (zcm->impl); return;
            case ZCM_NONBLOCKING: zcm_nonblocking_destroy(zcm->impl); return;
        }
#endif
        assert(zcm->type == ZCM_NONBLOCKING);
        zcm_nonblocking_destroy(zcm->impl);
    }
}

int zcm_errno(const zcm_t* zcm)
{
    return zcm->err;
}

static const char* errcode_str[] = {
    "Okay, no errors",                          /* ZCM_EOK      */
    "Invalid arguments",                        /* ZCM_EINVALID */
    "Resource unavailable, try again",          /* ZCM_EAGAIN   */
    "Transport connection failed",              /* ZCM_ECONNECT */
    "Operation was unexpectedly interrupted",   /* ZCM_EINTR    */
};

const char* zcm_strerror(const zcm_t* zcm)
{
    return zcm_strerrno(zcm->err);
}

const char* zcm_strerrno(int err)
{
    if (((unsigned) err) >= ZCM__RESERVED_COUNT) {
        return "Unknown error occurred";
    } else {
        return errcode_str[(unsigned) err];
    }
}

int zcm_publish(zcm_t* zcm, const char* channel, const uint8_t* data, uint32_t len)
{
#ifndef ZCM_EMBEDDED
    switch (zcm->type) {
        case ZCM_BLOCKING: {
            zcm->err = zcm_blocking_publish(zcm->impl, channel, data, len);
            return zcm->err;
        }
        case ZCM_NONBLOCKING: return zcm_nonblocking_publish(zcm->impl, channel, data, len);
    }
#endif
    assert(zcm->type == ZCM_NONBLOCKING);
    return zcm_nonblocking_publish(zcm->impl, channel, data, len);
}

void zcm_flush(zcm_t* zcm)
{
#ifndef ZCM_EMBEDDED
    switch (zcm->type) {
        case ZCM_BLOCKING:    return zcm_blocking_flush(zcm->impl);
        case ZCM_NONBLOCKING: return zcm_nonblocking_flush(zcm->impl);
    }
#endif
    assert(zcm->type == ZCM_NONBLOCKING);
    return zcm_nonblocking_flush(zcm->impl);
}

int  zcm_try_flush(zcm_t* zcm)
{
#ifndef ZCM_EMBEDDED
    switch (zcm->type) {
        case ZCM_BLOCKING:    return zcm_blocking_try_flush(zcm->impl);
        case ZCM_NONBLOCKING: zcm_nonblocking_flush(zcm->impl); return ZCM_EOK;
    }
#endif
    assert(zcm->type == ZCM_NONBLOCKING);
    zcm_nonblocking_flush(zcm->impl);
    return ZCM_EOK;
}

zcm_sub_t* zcm_subscribe(zcm_t* zcm, const char* channel, zcm_msg_handler_t cb, void* usr)
{
#ifndef ZCM_EMBEDDED
    switch (zcm->type) {
        case ZCM_BLOCKING:    return zcm_blocking_subscribe   (zcm->impl, channel, cb, usr);
        case ZCM_NONBLOCKING: return zcm_nonblocking_subscribe(zcm->impl, channel, cb, usr);
    }
#endif
    assert(zcm->type == ZCM_NONBLOCKING);
    return zcm_nonblocking_subscribe(zcm->impl, channel, cb, usr);
}

zcm_sub_t* zcm_try_subscribe(zcm_t* zcm, const char* channel, zcm_msg_handler_t cb, void* usr)
{
#ifndef ZCM_EMBEDDED
    switch (zcm->type) {
        case ZCM_BLOCKING:    return zcm_blocking_try_subscribe(zcm->impl, channel, cb, usr);
        case ZCM_NONBLOCKING: return zcm_nonblocking_subscribe (zcm->impl, channel, cb, usr);
    }
#endif
    assert(zcm->type == ZCM_NONBLOCKING);
    return zcm_nonblocking_subscribe(zcm->impl, channel, cb, usr);
}

int zcm_unsubscribe(zcm_t* zcm, zcm_sub_t* sub)
{
#ifndef ZCM_EMBEDDED
    switch (zcm->type) {
        case ZCM_BLOCKING:    return zcm_blocking_unsubscribe   (zcm->impl, sub);
        case ZCM_NONBLOCKING: return zcm_nonblocking_unsubscribe(zcm->impl, sub);
    }
#endif
    assert(zcm->type == ZCM_NONBLOCKING);
    return zcm_nonblocking_unsubscribe(zcm->impl, sub);
}

int zcm_try_unsubscribe(zcm_t* zcm, zcm_sub_t* sub)
{
#ifndef ZCM_EMBEDDED
    switch (zcm->type) {
        case ZCM_BLOCKING:    return zcm_blocking_try_unsubscribe(zcm->impl, sub);
        case ZCM_NONBLOCKING: return zcm_nonblocking_unsubscribe (zcm->impl, sub);
    }
#endif
    assert(zcm->type == ZCM_NONBLOCKING);
    return zcm_nonblocking_unsubscribe(zcm->impl, sub);
}

#ifndef ZCM_EMBEDDED
void zcm_start(zcm_t* zcm)
{
    switch (zcm->type) {
        case ZCM_BLOCKING:    return zcm_blocking_start(zcm->impl);
        case ZCM_NONBLOCKING: assert(0 && "Cannot start() on a nonblocking ZCM interface"); break;
    }
}
#endif

#ifndef ZCM_EMBEDDED
void zcm_stop(zcm_t* zcm)
{
    switch (zcm->type) {
        case ZCM_BLOCKING:    return zcm_blocking_stop(zcm->impl);
        case ZCM_NONBLOCKING: assert(0 && "Cannot stop() on a nonblocking ZCM interface"); break;
    }
}
#endif

#ifndef ZCM_EMBEDDED
int zcm_try_stop(zcm_t* zcm)
{
    switch (zcm->type) {
        case ZCM_BLOCKING:    return zcm_blocking_try_stop(zcm->impl);
        case ZCM_NONBLOCKING:
            assert(0 && "Cannot try_stop() on a nonblocking ZCM interface"); break;
    }
    return ZCM_EINVALID;
}
#endif

#ifndef ZCM_EMBEDDED
void zcm_run(zcm_t* zcm)
{
    switch (zcm->type) {
        case ZCM_BLOCKING:    return zcm_blocking_run(zcm->impl);
        case ZCM_NONBLOCKING: assert(0 && "Cannot run() on a nonblocking ZCM interface"); break;
    }
}
#endif

#ifndef ZCM_EMBEDDED
void zcm_pause(zcm_t* zcm)
{
    switch (zcm->type) {
        case ZCM_BLOCKING:    return zcm_blocking_pause(zcm->impl);
        case ZCM_NONBLOCKING: assert(0 && "Cannot pause() on a nonblocking ZCM interface"); break;
    }
}
#endif

#ifndef ZCM_EMBEDDED
void zcm_resume(zcm_t* zcm)
{
    switch (zcm->type) {
        case ZCM_BLOCKING:    return zcm_blocking_resume(zcm->impl);
        case ZCM_NONBLOCKING: assert(0 && "Cannot resume() on a nonblocking ZCM interface"); break;
    }
}
#endif

#ifndef ZCM_EMBEDDED
int zcm_handle(zcm_t* zcm)
{
    switch (zcm->type) {
        case ZCM_BLOCKING:    return zcm_blocking_handle(zcm->impl);
        case ZCM_NONBLOCKING: assert(0 && "Cannot handle() on a nonblocking ZCM interface"); break;
    }
    return -1;
}
#endif

#ifndef ZCM_EMBEDDED
void zcm_set_queue_size(zcm_t* zcm, uint32_t numMsgs)
{
    switch (zcm->type) {
        case ZCM_BLOCKING:    return zcm_blocking_set_queue_size(zcm->impl, numMsgs); break;
        case ZCM_NONBLOCKING:
            assert(0 && "Cannot set_queue_size() on a nonblocking ZCM interface"); break;
    }
}
#endif

int zcm_handle_nonblock(zcm_t* zcm)
{
#ifndef ZCM_EMBEDDED
    switch (zcm->type) {
        case ZCM_BLOCKING:
            assert(0 && "Cannot handle_nonblock() on a blocking ZCM interface"); break;
        case ZCM_NONBLOCKING: return zcm_nonblocking_handle_nonblock(zcm->impl); break;
    }
#endif
    assert(zcm->type == ZCM_NONBLOCKING);
    return zcm_nonblocking_handle_nonblock(zcm->impl);
}
