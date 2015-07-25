#include "transport.h"
#include "transport_zmq_ipc.h"
#include <string.h>

#include <stdio.h>

zcm_trans_t *zcm_trans_builtin_create(const char *transport)
{
    if (transport == NULL)
        return NULL;

    if (strcmp(transport, "ipc") == 0) {
        return zcm_trans_ipc_create();
    } else {
        return NULL;
    }
}
