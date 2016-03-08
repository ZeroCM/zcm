#include <iostream>
#include <sstream>
#include <string>
#include <getopt.h>

#include <zcm/zcm-cpp.hpp>

using namespace std;

struct Args
{
    double speed = 1.0;
    bool verbose = false;
    string zcmUrlOut = "";
    string filename = "";
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

        int c;
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
                    return false;
            };
        }

        if (optind != argc - 1) {
            cerr << "Please specify a logfile" << endl;
            return false;
        }

        filename = string(argv[optind]);

        std::stringstream ss;
        ss << "file://" << filename << "?speed=" << speed;
        zcmUrlIn = ss.str();

        return true;
    }
};

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

        cout << "Using playback speed " << args.speed << endl;

        zcmIn = new zcm::ZCM(args.zcmUrlIn);
        if (!zcmIn->good()) {
            cerr << "Error: Failed to open '" << args.filename << "'" << endl;
            return false;
        }

        zcmOut = new zcm::ZCM(args.zcmUrlOut);
        if (!zcmOut->good()) {
            fprintf(stderr, "Error: Failed to create output ZCM\n");
            return false;
        }

        return true;
    }

    void run()
    {
        zcmIn->subscribe(".*", &handler, this);

        zcmIn->run();

        delete zcmIn;
        delete zcmOut;
    }

    static void handler(const zcm::ReceiveBuffer *rbuf, const string& channel, void *usr)
    {
        LogPlayer* lp = (LogPlayer *) usr;
        if (lp->args.verbose)
            printf("%.3f Channel %-20s size %d\n", rbuf->recv_utime / 1000000.0,
                    channel.c_str(), rbuf->data_size);
       lp->zcmOut->publish(channel, rbuf->data, rbuf->data_size);
    }
};

void usage(char * cmd)
{
    fprintf(stderr, "usage: zcm-logplayer [options] [FILE]\n"
            "\n"
            "    Reads packets from an ZCM log file and publishes them to a \n"
            "    ZCM transport.\n"
            "\n"
            "Options:\n"
            "\n"
            "  -s, --speed=NUM     Playback speed multiplier.  Default is 1.\n"
            "  -u, --zcm-url=URL   Play logged messages on the specified ZCM URL.\n"
            "  -v, --verbose       Print information about each packet.\n"
            "  -h, --help          Shows some help text and exits.\n"
            "\n");
}

int main(int argc, char* argv[])
{
    LogPlayer lp;
    if (!lp.init(argc, argv)) {
        usage(argv[0]);
        return 1;
    }

    lp.run();
}
