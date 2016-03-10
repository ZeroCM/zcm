#include <iostream>
#include <sstream>
#include <string>
#include <getopt.h>
#include <atomic>
#include <signal.h>

#include <zcm/zcm-cpp.hpp>

using namespace std;

static atomic_int done {0};

static void sighandler(int signal)
{
    done++;
    if (done == 3) exit(1);
}

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

        zcmIn = new zcm::ZCM(args.zcmUrlIn);
        if (!zcmIn->good()) {
            cerr << "Error: Failed to open '" << args.filename << "'" << endl;
            return false;
        }

        zcmOut = new zcm::ZCM(args.zcmUrlOut);
        if (!zcmOut->good()) {
            cerr << "Error: Failed to create output ZCM" << endl;
            return false;
        }

        cout << "Using playback speed " << args.speed << endl;

        return true;
    }

    void run()
    {
        zcmIn->subscribe(".*", &handler, this);

        zcmIn->start();

        while (!done) usleep(1e6);

        zcmIn->stop();
    }

    static void handler(const zcm::ReceiveBuffer *rbuf, const string& channel, void *usr)
    {
        LogPlayer* lp = (LogPlayer *) usr;
        if (lp->args.verbose)
            printf("%.3f Channel %-20s size %d\n", rbuf->recv_utime / 1e6,
                    channel.c_str(), rbuf->data_size);
       lp->zcmOut->publish(channel, rbuf->data, rbuf->data_size);
    }
};

void usage(char * cmd)
{
    cerr << "usage: zcm-logplayer [options] [FILE]" << endl
         << "" << endl
         << "    Reads packets from an ZCM log file and publishes them to a " << endl
         << "    ZCM transport." << endl
         << "" << endl
         << "Options:" << endl
         << "" << endl
         << "  -s, --speed=NUM     Playback speed multiplier.  Default is 1." << endl
         << "  -u, --zcm-url=URL   Play logged messages on the specified ZCM URL." << endl
         << "  -v, --verbose       Print information about each packet." << endl
         << "  -h, --help          Shows some help text and exits." << endl
         << endl;
}

int main(int argc, char* argv[])
{
    LogPlayer lp;
    if (!lp.init(argc, argv)) {
        usage(argv[0]);
        return 1;
    }

    // Register signal handlers
    signal(SIGINT, sighandler);
    signal(SIGQUIT, sighandler);
    signal(SIGTERM, sighandler);

    lp.run();

    cout << "zcm-logplayer exiting" << endl;

    return 0;
}
