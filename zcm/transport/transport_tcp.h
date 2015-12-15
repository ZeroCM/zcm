#ifndef _ZCM_TRANSPORT_TCP_H
#define _ZCM_TRANSPORT_TCP_H

#include "zcm/transport.h"

#ifdef __cplusplus
extern "C" {
#endif

zcm_server_t *transport_tcp_server_create(zcm_url_t *url);

#ifdef __cplusplus
}
#endif

#endif /* _ZCM_TRANSPORT_TCP_H */
