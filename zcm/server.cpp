#include "zcm/server.h"
#include "zcm/url.h"
#include <cstring>

// TODO implement a server transport registrar rather than this hardcoded
//      table. Currently, users cannot added their own server-style transports.
#include "transport/transport_tcp.h"
typedef zcm_server_t *(server_create_func)(zcm_url_t *url);
struct entry_t
{
    const char *name;
    server_create_func *creator;
};

entry_t entries[] = {
    {"tcp", transport_tcp_server_create},
};

/////////////// C Interface Functions ////////////////
extern "C" {

zcm_server_t *zcm_server_create(const char *url)
{
    zcm_url_t *u = zcm_url_create(url);
    if (!u)
        return NULL;

    const char *protocol = zcm_url_protocol(u);
    for (size_t i = 0; i < sizeof(entries)/sizeof(entries[0]); i++) {
        if (strcmp(protocol, entries[i].name) == 0) {
            return entries[i].creator(u);
        }
    }

    return NULL;
}

zcm_t *zcm_server_accept(zcm_server_t *svr, int timeout)
{
    return svr->vtbl->accept(svr, timeout);
}

void zcm_server_destroy(zcm_server_t *svr)
{
    svr->vtbl->destroy(svr);
}

}
