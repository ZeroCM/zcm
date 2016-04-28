#include <iostream>
#include <string>
#include <vector>
#include <atomic>
#include <signal.h>

#include <getopt.h>

#include "zcm/zcm-cpp.hpp"

using namespace std;

static atomic_int done {0};

static void sighandler(int signal)
{
    done++;
    if (done == 3) exit(1);
}

struct Args
{
    vector<string> Achannels;
    vector<string> Bchannels;

    string Aurl = "";
    string Burl = "";

    bool parse(int argc, char *argv[])
    {
        // set some defaults
        const char *optstring = "hA:B:a:b:";
        struct option long_opts[] = {
            { "help", no_argument, 0, 'h' },
            { "A-endpt", required_argument, 0, 'A'},
            { "B-endpt", required_argument, 0, 'B'},
            { "A-channel", required_argument, 0, 'a' },
            { "B-channel", required_argument, 0, 'b' },
            { 0, 0, 0, 0 }
        };

        int c;
        while ((c = getopt_long (argc, argv, optstring, long_opts, 0)) >= 0) {
            switch (c) {
                case 'A':
                    Aurl = optarg;
                    break;
                case 'B':
                    Burl = optarg;
                    break;
                case 'a':
                    Achannels.push_back(optarg);
                    break;
                case 'b':
                    Bchannels.push_back(optarg);
                    break;
                case 'h':
                default:
                    return false;
            };
        }

        if (Aurl == "") {
            cerr << "Please specify A endpt url with the -A option" << endl;
            return false;
        }

        if (Burl == "") {
            cerr << "Please specify B endpt url with the -B option" << endl;
            return false;
        }

        if (Aurl == Burl) {
            cerr << "A and B endpoint urls must be unique" << endl;
            return false;
        }

        return true;
    }
};

struct Bridge
{
    Args   args;

    zcm::ZCM *zcmA = nullptr;
    zcm::ZCM *zcmB = nullptr;

    Bridge() {}

    ~Bridge()
    {
        if (zcmA)
            delete zcmA;
        if (zcmB)
            delete zcmB;
    }

    bool init(int argc, char *argv[])
    {
        if (!args.parse(argc, argv))
            return false;

        // A endpoint network
        zcmA = new zcm::ZCM(args.Aurl);
        if (!zcmA->good()) {
            cerr << "Couldn't initialize A endpoint ZCM network! "
                    "Please check your A endpoint transport url." << endl << endl;
            delete zcmA;
            zcmA = nullptr;
            return false;
        }

        // B endpoint network
        zcmB = new zcm::ZCM(args.Burl);
        if (!zcmB->good()) {
            cerr << "Couldn't initialize B endpoint ZCM network! "
                    "Please check your B endpoint transport url." << endl << endl;
            delete zcmB;
            zcmB = nullptr;
            return false;
        }

        return true;
    }

    static void handler(const zcm::ReceiveBuffer* rbuf, const string& channel, void* usr)
    {
        ((zcm::ZCM*)usr)->publish(channel, rbuf->data, rbuf->data_size);
    }

    void run()
    {
        if (args.Achannels.size() == 0) {
            zcmA->subscribe(".*", &handler, zcmB);
        } else {
            for (auto iter : args.Achannels)
                zcmA->subscribe(iter, &handler, zcmB);
        }

        if (args.Bchannels.size() == 0) {
            zcmB->subscribe(".*", &handler, zcmA);
        } else {
            for (auto iter : args.Bchannels)
                zcmB->subscribe(iter, &handler, zcmA);
        }

        zcmA->start();
        zcmB->start();

        while (!done) usleep(1e6);

        zcmA->stop();
        zcmB->stop();

        zcmA->flush();
        zcmB->flush();
    }
};

static void usage()
{
    cerr << "usage: zcm-bridge [options]" << endl
         << "" << endl
         << "    ZCM bridge utility. Bridges all data from one ZCM network to another." << endl
         << "" << endl
         << "Example:" << endl
         << "    zcm-bridge -A ipc -B udpm://239.255.76.67:7667?ttl=0" << endl
         << "" << endl
         << "Options:" << endl
         << "" << endl
         << "  -h, --help                 Shows this help text and exits" << endl
         << "  -A, --A-enpt=URL           One end of the bridge. Ex: zcm-bridge -A ipc" << endl
         << "  -B, --B-endpt=URL          One end of the bridge. Ex: zcm-bridge -B udpm://239.255.76.67:7667?ttl=0" << endl
         << "  -a, --A-channel=CHANNEL    One channel to subscribe to on A and repeat to B." << endl
         << "                             This argument can be specified multiple times. If this option is not," << endl
         << "                             present then we subscribe to all messages on the A interface." << endl
         << "                             Ex: zcm-bridge -A ipc -a EXAMPLE -B udpm://239.255.76.67:7667?ttl=0" << endl
         << "  -b, --B-channel=CHANNEL    One channel to subscribe to on B and repeat to A." << endl
         << "                             This argument can be specified multiple times. If this option is not," << endl
         << "                             present then we subscribe to all messages on the B interface." << endl
         << "                             Ex: zcm-bridge -A ipc -B udpm://239.255.76.67:7667?ttl=0 -b EXAMPLE" << endl
         << "" << endl << endl;
}

int main(int argc, char *argv[])
{
    Bridge bridge{};
    if (!bridge.init(argc, argv)) {
        usage();
        return 1;
    }

    // Register signal handlers
    signal(SIGINT, sighandler);
    signal(SIGQUIT, sighandler);
    signal(SIGTERM, sighandler);

    bridge.run();

    cerr << "Bridge exiting" << endl;

    return 0;
}
