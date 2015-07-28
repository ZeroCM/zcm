#include "zcm/transport.h"
#include "zcm/transport/transport_zmq_local.h"
#include "zcm/transport/transport_serial.h"
#include "zcm/util/url.h"

#include <string.h>
#include <stdio.h>

zcm_trans_t *zcm_trans_builtin_create(const char *url)
{
    if (url == NULL)
        return NULL;

    zcm_url_t *u = zcm_url_create(url);
    const char *protocol = zcm_url_protocol(u);
    //const char *address = zcm_url_address(u);

    zcm_trans_t *ret = NULL;
    if (strcmp(protocol, "ipc") == 0) {
        ret = zcm_trans_ipc_create(u);
    } else if (strcmp(protocol, "inproc") == 0) {
        ret = zcm_trans_inproc_create(u);
    } else if (strcmp(protocol, "serial") == 0) {
        ret = zcm_trans_serial_create(u);
    }

    zcm_url_destroy(u);
    return ret;
}
