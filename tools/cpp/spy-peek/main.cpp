#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <getopt.h>
#include <inttypes.h>

#include "zcm/zcm.h"

volatile int done = 0;
static bool verbose;

static void sighandler(int code)
{
    done++;
    if (done >= 3) exit(1);
}

static void handler(const zcm_recv_buf_t *rbuf, const char *channel,
                    void *ser)
{
    printf("Message received on channel: \"%s\"\n", channel);
    if (verbose) {
        printf("Raw data: ");
        for (size_t i = 0; i < rbuf->data_size; ++i) {
            printf("%x ", (uint8_t)rbuf->data[i]);
        }
        printf("\n");
    }
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
            "  -v, --verbose              Print raw bytes of zcm data for each msg\n"
            "\n");
}

static const char *zcmurl = nullptr;
static bool parse_args(int argc, char *argv[])
{
    // set some defaults
    const char *optstring = "hu:v";
    struct option long_opts[] = {
        { "help",    no_argument,       0, 'h' },
        { "zcm-url", required_argument, 0, 'u' },
        { "verbose", no_argument,       0, 'v' },
        { 0, 0, 0, 0 }
    };

    int c;
    while ((c = getopt_long (argc, argv, optstring, long_opts, 0)) >= 0) {
        switch (c) {
            case 'u': zcmurl  = optarg; break;
            case 'v': verbose = true;   break;
            case 'h': default: usage(); return false;
        };
    }

    return true;
}

int main(int argc, char *argv[])
{
    if (!parse_args(argc, argv)) return 1;

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
