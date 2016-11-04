#include <iostream>
#include <string>
#include <unistd.h>
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

    vector<int> Adec;
    vector<int> Bdec;

    string Aurl = "";
    string Burl = "";

    string Aprefix = "";
    string Bprefix = "";

    bool parse(int argc, char *argv[])
    {
        // set some defaults
        const char *optstring = "hp:r:A:B:a:b:d:";
        struct option long_opts[] = {
            { "help",       no_argument, 0, 'h' },
            { "prefix-a",   required_argument, 0, 'p' },
            { "prefix-b",   required_argument, 0, 'r' },
            { "A-endpt",    required_argument, 0, 'A' },
            { "B-endpt",    required_argument, 0, 'B' },
            { "A-channel",  required_argument, 0, 'a' },
            { "B-channel",  required_argument, 0, 'b' },
            { "decimation", required_argument, 0, 'd' },
            { 0, 0, 0, 0 }
        };

        int c;
        vector<int> *currDec = nullptr;
        while ((c = getopt_long (argc, argv, optstring, long_opts, 0)) >= 0) {
            switch (c) {
                case 'p': currDec = nullptr; Aprefix = optarg; break;
                case 'r': currDec = nullptr; Bprefix = optarg; break;
                case 'A': currDec = nullptr; Aurl = optarg; break;
                case 'B': currDec = nullptr; Burl = optarg; break;
                case 'a':
                    Achannels.push_back(optarg);
                    Adec.push_back(0);
                    currDec = &Adec;
                    break;
                case 'b':
                    Bchannels.push_back(optarg);
                    Bdec.push_back(0);
                    currDec = &Bdec;
                    break;
                case 'd':
                    ZCM_ASSERT(currDec != nullptr &&
                               "Decimation must immediately follow a channel");
                    currDec->back() = atoi(optarg);
                    currDec = nullptr;
                    break;
                case 'h': default: return false;
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

    struct BridgeInfo {
        zcm::ZCM *zcmOut = nullptr;
        string    prefix = "";
        int       decimation = 0;
        int       nSkipped = 0;

        BridgeInfo(zcm::ZCM* zcmOut, const string &prefix, int dec) :
            zcmOut(zcmOut), prefix(prefix), decimation(dec), nSkipped(0) {}

        BridgeInfo() {}
    };

    Bridge() {}

    ~Bridge()
    {
        if (zcmA) delete zcmA;
        if (zcmB) delete zcmB;
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
        BridgeInfo* info = (BridgeInfo*)usr;
        if (info->nSkipped == info->decimation) {
            string newChannel = info->prefix + channel;
            info->zcmOut->publish(newChannel, rbuf->data, rbuf->data_size);
            info->nSkipped = 0;
        } else {
            info->nSkipped++;
        }
    }

    void run()
    {
        vector<BridgeInfo> infoA, infoB;

        infoA.reserve(args.Achannels.size());
        infoB.reserve(args.Bchannels.size());

        BridgeInfo defaultA(zcmB, args.Bprefix, 0),
                   defaultB(zcmA, args.Aprefix, 0);

        if (args.Achannels.size() == 0) {
            zcmA->subscribe(".*", &handler, &defaultA);
        } else {
            for (size_t i = 0; i < args.Achannels.size(); i++) {
                infoA.emplace_back(zcmB, args.Bprefix, args.Adec.at(i));
                zcmA->subscribe(args.Achannels.at(i), &handler, &infoA.back());
            }
        }

        if (args.Bchannels.size() == 0) {
            zcmB->subscribe(".*", &handler, &defaultB);
        } else {
            for (size_t i = 0; i < args.Bchannels.size(); i++) {
                infoB.emplace_back(zcmA, args.Aprefix, args.Bdec.at(i));
                zcmB->subscribe(args.Bchannels.at(i), &handler, &infoB.back());
            }
        }

        ZCM_ASSERT(infoA.size() == args.Achannels.size() &&
                   infoA.size() == args.Adec.size() &&
                   infoB.size() == args.Bchannels.size() &&
                   infoB.size() == args.Bdec.size());

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
         << "  -p, --prefix-a             Specify a prefix for all messages published on the A url" << endl
         << "  -r, --prefix-b             Specify a prefix for all messages published on the B url" << endl
         << "  -a, --A-channel=CHANNEL    One channel to subscribe to on A and repeat to B." << endl
         << "                             This argument can be specified multiple times. If this option is not," << endl
         << "                             present then we subscribe to all messages on the A interface." << endl
         << "                             Ex: zcm-bridge -A ipc -a EXAMPLE -B udpm://239.255.76.67:7667?ttl=0" << endl
         << "  -b, --B-channel=CHANNEL    One channel to subscribe to on B and repeat to A." << endl
         << "                             This argument can be specified multiple times. If this option is not," << endl
         << "                             present then we subscribe to all messages on the B interface." << endl
         << "                             Ex: zcm-bridge -A ipc -B udpm://239.255.76.67:7667?ttl=0 -b EXAMPLE" << endl
         << "  -d, --decimation           Decimation level for the preceeding A-channel or B-channel. " << endl
         << "                             Ex: zcm-bridge -A ipc -B udpm://239.255.76.67:7667?ttl=0 -b EXAMPLE -d 2" << endl
         << "                             This example would result in the message on EXAMPLE being rebroadcast on" << endl
         << "                             the A url every third message." << endl
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
