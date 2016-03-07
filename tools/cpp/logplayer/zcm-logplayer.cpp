#include <iostream>
#include <string>
#include <getopt.h>

#include <zcm/zcm.h>

struct Args
{
    double speed = 1.0;
    bool verbose = false;
    string zcmUrlOut = "";
    string zcmUrlIn = "";

    bool init(int argc, char *argv[])
    {
        struct option long_opts[] = {
            { "help", no_argument, 0, 'h' },
            { "speed", required_argument, 0, 's' },
            { "zcm-url", required_argument, 0, 'u' },
            { "verbose", no_argument, 0, 'v' },
            { 0, 0, 0, 0 }
        };

        while ((c = getopt_long(argc, argv, "hs:vu:", long_opts, 0)) >= 0)
        {
            switch (c) {
                case 's':
                    speed = strtod(optarg, NULL);
                    break;
                case 'u':
                    zcmUrlOut = string(optarg);
                    break;
                case 'v':
                    verbose = true;
                    break;
                case 'h':
                default:
                    usage (argv[0]);
                    return false;
            };
        }

        if (optind != argc - 1) {
            usage (argv[0]);
            return false;
        }

        string filename = string(argv[optind]);

        zcmUrlIn = string("file://") + filename + string("?speed=") + speed;
    }
}

typedef struct logplayer logplayer_t;
struct LogPlayer
{
    Args args;
    zcm::ZCM *zcmIn = nullptr;
    zcm::ZCM *zcmOut = nullptr;

    LogPlayer() { }

    ~LogPlayer()
    {
        if (zcmIn) delete zcmIn;
        if (zcmOut) delete zcmOut;
    }

    bool init(int argc, char *argv[])
    {
        if (!args.init(argc, argv))
            return false;

        printf("Using playback speed %f\n", args.speed);

        zcmIn = new zcm::ZCM(args.zcmUrlIn);
        if (!zcmIn.good()) {
            fprintf(stderr, "Error: Failed to open %s\n", file);
            return false;
        }

        zcmOut = new zcm::ZCM(args.zcmUrlOut);
        if (!zcmOut.good()) {
            fprintf(stderr, "Error: Failed to create output ZCM\n");
            return false;
        }
    }

    void run()
    {
        zcmIn.subscribe(".*", &handler, this);

        zcmIn.run();

        delete zcmIn;
        delete zcmOut;
    }

    static void handler(const zcm_recv_buf_t *rbuf, const char *channel, void *usr)
    {
        LogPlayer* lp = (LogPlayer *) u;
        if (lp->verbose)
            printf ("%.3f Channel %-20s size %d\n", rbuf->recv_utime / 1000000.0,
                    channel, rbuf->data_size);
        zcm_publish(lp->zcmOut, channel, rbuf->data, rbuf->data_size);
    }
};

static void usage (char * cmd)
{
    fprintf (stderr, "\
Usage: %s [OPTION...] FILE\n\
  Reads packets from an ZCM log file and publishes them to ZCM.\n\
\n\
Options:\n\
  -v, --verbose       Print information about each packet.\n\
  -s, --speed=NUM     Playback speed multiplier.  Default is 1.0.\n\
  -u, --zcm-url=URL   Play logged messages on the specified ZCM URL.\n\
  -h, --help          Shows some help text and exits.\n\
  \n", cmd);
}

int
main(int argc, char* argv[])
{
    LogPlayer lp;
    if (!lp.init(argc, argv)) {
        cerr << "Unable to initialize zcm-logplayer" << endl;
        return 1;
    }

    lp.run();
}
