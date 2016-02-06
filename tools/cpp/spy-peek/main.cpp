#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <getopt.h>
#include <inttypes.h>

#include "zcm/zcm.h"

volatile int done = 0;

static void sighandler(int code)
{
    done++;
    if (done >= 3) exit(1);
}

static void handler(const zcm_recv_buf_t *rbuf, const char *channel,
                    void *ser)
{
    printf("Message received on channel: \"%s\"\n", channel);
}


static const char *zcmurl = nullptr;
static bool parse_args(int argc, char *argv[])
{
    // set some defaults
    const char *optstring = "hu:";
    struct option long_opts[] = {
        { "help", no_argument, 0, 'h' },
        { "zcm-url", required_argument, 0, 'u' },
        { 0, 0, 0, 0 }
    };

    int c;
    while ((c = getopt_long (argc, argv, optstring, long_opts, 0)) >= 0) {
        switch (c) {
            case 'u':
                zcmurl = optarg;
                break;
            case 'h':
            default:
                return false;
        };
    }

    return true;
}

static void usage()
{
    fprintf(stderr, "usage: zcm-repeater [options]\n"
            "\n"
            "    Terminal based spy utility.  Subscribes to all channels on a ZCM\n"
            "    transport and displays them in an interactive terminal.\n"
            "Example:\n"
            "    zcm-spy-lite\n"
            "\n"
            "Options:\n"
            "\n"
            "  -h, --help                 Shows this help text and exits\n"
            "  -u, --zcm-url=URL          Log messages on the specified ZCM URL\n"
            "\n");
}

int main(int argc, char *argv[])
{
    if (!parse_args(argc, argv)) {
        usage();
        return 1;
    }

    zcm_t *zcm = zcm_create(zcmurl);
    if (!zcm) {
        fprintf(stderr, "Couldn't initialize ZCM! Try providing a URL with the "
                        "-u opt or setting the ZCM_DEFAULT_URL envvar\n");
        return 1;
    }

    zcm_sub_t *subs = zcm_subscribe(zcm, ".*", &handler, NULL);

    signal(SIGINT,  sighandler);
    signal(SIGQUIT, sighandler);
    signal(SIGTERM, sighandler);

    zcm_start(zcm);

    while (!done) usleep(500000);

    zcm_stop(zcm);
    zcm_unsubscribe(zcm, subs);
    zcm_destroy(zcm);
    return 0;
}
