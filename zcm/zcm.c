
/*******************************************************************************
 * ZCM Core API
 *
 *     This file implements the core ZCM API for both blocking and non-blocking modes
 *     The code is a bit hairy here because it must be C89 and be able to conditionally
 *     build only non-blocking. These requirements are needed to support embedded.
 *
 *******************************************************************************/

#include "zcm/zcm.h"
#include "zcm/zcm_private.h"
#include "zcm/nonblocking.h"

#ifndef ZCM_EMBEDDED
#include <stdlib.h>
#include <string.h>

# include "zcm/blocking.h"
# include "zcm/transport_registrar.h"
# include "zcm/util/debug.h"
#else
/** Note: here's to hoping that variadic macros are "portable enough" **/
# define ZCM_DEBUG(...)
#endif

#ifndef ZCM_EMBEDDED
int zcm_retcode_name_to_enum(const char* zcm_retcode_name)
{
    #define X(n, v, s) if (strcmp(#n, zcm_retcode_name) == 0) return v;
    ZCM_RETURN_CODES
    #undef X
    return ZCM_NUM_RETURN_CODES;
}
#endif

#ifndef ZCM_EMBEDDED
zcm_t* zcm_create_from_url(const char* url)
{
    zcm_t* z = NULL;
    ZCM_ASSERT(zcm_try_create_from_url(&z, url) == ZCM_EOK);
    ZCM_ASSERT(z);
    return z;
}

int zcm_try_create_from_url(zcm_t** z, const char* url)
{
    *z = malloc(sizeof(zcm_t));
    if (!*z) return ZCM_EMEMORY;
    int init_return_code = zcm_init(*z, url);
    if (init_return_code != ZCM_EOK) {
        free(*z);
        *z = NULL;
        return init_return_code;
    }
    return ZCM_EOK;
}
#endif

zcm_t* zcm_create_trans(zcm_trans_t* zt)
{
    zcm_t* z = NULL;
    ZCM_ASSERT(zcm_try_create_trans(&z, zt) == ZCM_EOK);
    ZCM_ASSERT(z);
    return z;
}


int zcm_try_create_trans(zcm_t** z, zcm_trans_t* zt)
{
    *z = malloc(sizeof(zcm_t));
    if (!*z) return ZCM_EMEMORY;
    int init_trans_ret = zcm_init_trans(*z, zt);
    if (init_trans_ret == ZCM_EOK) {
        free(*z);
        *z = NULL;
        return init_trans_ret;
    }
    return ZCM_EOK;
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
            return ZCM_EUNKNOWN;
        }
    }
    int ret = ZCM_EUNKNOWN;
    zcm_url_t* u = zcm_url_create(url);
    if (!u) return ZCM_EMEMORY;
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
    /* Check for valid transport */
    if (zt == NULL) {
        zcm->impl = NULL;
        zcm->err = ZCM_ECONNECT;
        return ZCM_ECONNECT;
    }

    /* Create blocking transport */
    if (zt->trans_type == ZCM_BLOCKING) {
        #ifndef ZCM_EMBEDDED
            zcm->type = ZCM_BLOCKING;
            /* RRR: do we want to use a "try_create" function here? */
            zcm->impl = zcm_blocking_create(zcm, zt);
        #else
            zcm->err = ZCM_EINVALID;
            return ZCM_EINVALID;
        #endif
    }

    /* Create non-blocking transport */
    if (zt->trans_type == ZCM_NONBLOCKING) {
        zcm->type = ZCM_NONBLOCKING;
        /* RRR: do we want to use a "try_create" function here? */
        zcm->impl = zcm_nonblocking_create(zcm, zt);
    }

    /* Check if transport has been initialized properly */
    if (!zcm->impl) {
        zcm->err = ZCM_EMEMORY;
        return ZCM_EMEMORY;
    }

    zcm->err = ZCM_EOK;
    return ZCM_EOK;
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
        zcm_nonblocking_destroy(zcm->impl);
    }
}

int zcm_errno(const zcm_t* zcm)
{
    return zcm->err;
}

static const char* errcode_str[] = {
    #define X(n, v, s) s,
    ZCM_RETURN_CODES
    #undef X
};

const char* zcm_strerror(const zcm_t* zcm)
{
    return zcm_strerrno(zcm->err);
}

const char* zcm_strerrno(int err)
{
    if (((unsigned) err) >= ZCM_NUM_RETURN_CODES) err = ZCM_NUM_RETURN_CODES;

    return errcode_str[(unsigned) err];
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
    return zcm_nonblocking_flush(zcm->impl);
}

int  zcm_flush_nonblock(zcm_t* zcm)
{
#ifndef ZCM_EMBEDDED
    switch (zcm->type) {
        case ZCM_BLOCKING:    return zcm_blocking_try_flush(zcm->impl);
        case ZCM_NONBLOCKING: zcm_nonblocking_flush(zcm->impl); return ZCM_EOK;
    }
#endif
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
    return zcm_nonblocking_unsubscribe(zcm->impl, sub);
}

#ifndef ZCM_EMBEDDED
void zcm_start(zcm_t* zcm)
{
    return zcm_blocking_start(zcm->impl);
}
#endif

#ifndef ZCM_EMBEDDED
void zcm_stop(zcm_t* zcm)
{
    return zcm_blocking_stop(zcm->impl);
}
#endif

#ifndef ZCM_EMBEDDED
int zcm_try_stop(zcm_t* zcm)
{
    return zcm_blocking_try_stop(zcm->impl);
}
#endif

#ifndef ZCM_EMBEDDED
void zcm_run(zcm_t* zcm)
{
    return zcm_blocking_run(zcm->impl);
}
#endif

#ifndef ZCM_EMBEDDED
void zcm_pause(zcm_t* zcm)
{
    return zcm_blocking_pause(zcm->impl);
}
#endif

#ifndef ZCM_EMBEDDED
void zcm_resume(zcm_t* zcm)
{
    return zcm_blocking_resume(zcm->impl);
}
#endif

#ifndef ZCM_EMBEDDED
int zcm_handle(zcm_t* zcm)
{
    return zcm_blocking_handle(zcm->impl);
}
#endif

#ifndef ZCM_EMBEDDED
void zcm_set_queue_size(zcm_t* zcm, uint32_t numMsgs)
{
    return zcm_blocking_set_queue_size(zcm->impl, numMsgs);
}
#endif

#ifndef ZCM_EMBEDDED
int  zcm_try_set_queue_size(zcm_t* zcm, uint32_t numMsgs)
{
    return zcm_blocking_try_set_queue_size(zcm->impl, numMsgs);
}
#endif

int zcm_handle_nonblock(zcm_t* zcm)
{
    return zcm_nonblocking_handle_nonblock(zcm->impl);
}
