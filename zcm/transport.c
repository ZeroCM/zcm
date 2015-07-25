#include "zcm/transport.h"
#include "zcm/transport/transport_zmq_local.h"
#include "zcm/transport/transport_serial.h"

#include <string.h>
#include <stdio.h>

zcm_trans_t *zcm_trans_builtin_create(const char *transport)
{
    if (transport == NULL)
        return NULL;

    if (strcmp(transport, "ipc") == 0) {
        return zcm_trans_ipc_create();
    } else if (strcmp(transport, "inproc") == 0) {
        return zcm_trans_inproc_create();
    } else if (strcmp(transport, "serial") == 0) {
        return zcm_trans_serial_create();
    } else {
        return NULL;
    }
}
