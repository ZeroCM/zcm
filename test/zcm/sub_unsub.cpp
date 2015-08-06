#include <zcm/zcm.h>
#include <zcm/transport.h>
#include <zcm/transport_registrar.h>

#include <iostream>
using namespace std;

static void generic_handler(const zcm_recv_buf_t *rbuf, const char *channel, void *usr)
{

}

int main(int argc, const char *argv[])
{
    zcm_transport_help(stdout);

    zcm_t *zcm_ipc = zcm_create("ipc");
    cout << "Trying to create zcm" << endl;
    if (!zcm_ipc)
        return 1;

    zcm_subscribe(zcm_ipc, "TEST", generic_handler, NULL);

    cout << "Cleaning up zcm struct" << endl;
    zcm_destroy(zcm_ipc);
    return 0;
}
