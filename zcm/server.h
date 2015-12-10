// An interface for server-oriented transport methods (e.g. TCP)
#ifndef _ZCM_SERVER_H
#define _ZCM_SERVER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "zcm.h"

/* Forward typedef'd structs */
typedef struct zcm_server_t zcm_server_t;

/* Create a new server and bind it on according to the url provided
    Returns NULL on failure */
zcm_server_t *zcm_server_create(const char *url);

/* Accept a connection on this server within the next 'timeout' millis
   A timeout of -1 means 'block-indefinately'
   Returns a new zcm_t* on success (must be cleaned up with zcm_destory)
   Return NULL on failure */
zcm_t *zcm_server_accept(zcm_server_t *server, long timeout);

/* Shutdown the server */
void zcm_server_destroy(zcm_server_t *server);


#ifdef __cplusplus
}
#endif

#endif /* _ZCM_SERVER_H */
