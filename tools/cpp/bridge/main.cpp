#include <iostream>
#include <string>
#include <unistd.h>
#include <vector>
#include <atomic>
#include <signal.h>

#include <getopt.h>

#include "zcm/zcm-cpp.hpp"
#include "zcm/zcm_coretypes.h"

#include "util/TranscoderPluginDb.hpp"

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

    string plugin_path = "";

    bool   debug = false;

    bool parse(int argc, char *argv[])
    {
        // set some defaults
        const char *optstring = "hA:B:a:b:D:p:d";
        struct option long_opts[] = {
            { "help",              no_argument, 0,  'h' },
            { "A-prefix",    required_argument, 0,   0  },
            { "B-prefix",    required_argument, 0,   0  },
            { "A-endpt",     required_argument, 0,  'A' },
            { "B-endpt",     required_argument, 0,  'B' },
            { "A-channel",   required_argument, 0,  'a' },
            { "B-channel",   required_argument, 0,  'b' },
            { "decimation",  required_argument, 0,  'D' },
            { "plugin-path", required_argument, 0,  'p' },
            { "debug",             no_argument, 0,  'd' },
            { 0, 0, 0, 0 }
        };

        int c;
        vector<int> *currDec = nullptr;
        int option_index;
        while ((c = getopt_long(argc, argv, optstring, long_opts, &option_index)) >= 0) {
            switch (c) {
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
                case 'D':
                    ZCM_ASSERT(currDec != nullptr &&
                               "Decimation must immediately follow a channel");
                    currDec->back() = atoi(optarg);
                    currDec = nullptr;
                    break;
                case 'p':
                    plugin_path = string(optarg);
                    break;
                case 'd':
                    debug = true;
                    break;
                case 0:
                    if (string(long_opts[option_index].name) == "A-prefix") {
                        currDec = nullptr;
                        Aprefix = optarg;
                    } else if (string(long_opts[option_index].name) == "B-prefix") {
                        currDec = nullptr;
                        Bprefix = optarg;
                    }
                    break;
                case 'h': default: usage(); return false;
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

        if (Aurl == Burl && (Aprefix == "" || Bprefix == "")) {
            cerr << "A and B endpoint urls must be unique if not using prefices" << endl;
            return false;
        }

        return true;
    }

    void usage()
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
             << "      --A-prefix=PREFIX      Specify a prefix for all messages published on the A url" << endl
             << "      --B-prefix=PREFIX      Specify a prefix for all messages published on the B url" << endl
             << "  -a, --A-channel=CHANNEL    One channel to subscribe to on A and repeat to B." << endl
             << "                             This argument can be specified multiple times. If this option is not," << endl
             << "                             present then we subscribe to all messages on the A interface." << endl
             << "                             Ex: zcm-bridge -A ipc -a EXAMPLE -B udpm://239.255.76.67:7667?ttl=0" << endl
             << "  -b, --B-channel=CHANNEL    One channel to subscribe to on B and repeat to A." << endl
             << "                             This argument can be specified multiple times. If this option is not," << endl
             << "                             present then we subscribe to all messages on the B interface." << endl
             << "                             Ex: zcm-bridge -A ipc -B udpm://239.255.76.67:7667?ttl=0 -b EXAMPLE" << endl
             << "  -D, --decimation           Decimation level for the preceeding A-channel or B-channel. " << endl
             << "                             Ex: zcm-bridge -A ipc -B udpm://239.255.76.67:7667?ttl=0 -b EXAMPLE -d 2" << endl
             << "                             This example would result in the message on EXAMPLE being rebroadcast on" << endl
             << "                             the A url every third message." << endl
             << "  -p, --plugin-path=path     Path to shared library containing transcoder plugins" << endl
             << "" << endl << endl;
    }
};

struct Bridge
{
    Args   args;

    zcm::ZCM *zcmA = nullptr;
    zcm::ZCM *zcmB = nullptr;

    static TranscoderPluginDb* pluginDb;
    static vector<zcm::TranscoderPlugin*> plugins;

    struct BridgeInfo
    {
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
        if (pluginDb) { delete pluginDb; pluginDb = nullptr; }
    }

    bool init(int argc, char *argv[])
    {
        if (!args.parse(argc, argv))
            return false;

        // A endpoint network
        zcmA = new zcm::ZCM(args.Aurl);
        if (!zcmA->good()) {
            cerr << "Couldn't initialize A endpoint ZCM network! "
                 << "Please check your A endpoint transport url: "
                 << args.Aurl
                 << endl << endl;
            delete zcmA;
            zcmA = nullptr;
            return false;
        }

        // B endpoint network
        zcmB = new zcm::ZCM(args.Burl);
        if (!zcmB->good()) {
            cerr << "Couldn't initialize B endpoint ZCM network! "
                 << "Please check your B endpoint transport url: "
                 << args.Burl
                 << endl << endl;
            delete zcmB;
            zcmB = nullptr;
            return false;
        }

        assert(pluginDb == nullptr);
        // Load plugins from path if specified
        if (args.plugin_path != "") {
            pluginDb = new TranscoderPluginDb(args.plugin_path, args.debug);
            vector<const zcm::TranscoderPlugin*> dbPlugins = pluginDb->getPlugins();
            if (dbPlugins.empty()) {
                cerr << "Couldn't find any plugins. Aborting." << endl;
                return false;
            }
            vector<string> dbPluginNames = pluginDb->getPluginNames();
            for (size_t i = 0; i < dbPlugins.size(); ++i) {
                plugins.push_back((zcm::TranscoderPlugin*) dbPlugins[i]);
                if (args.debug) cout << "Loaded plugin: " << dbPluginNames[i] << endl;
            }
        }

        return true;
    }

    static void handler(const zcm::ReceiveBuffer* rbuf, const string& channel, void* usr)
    {
        BridgeInfo* info = (BridgeInfo*)usr;
        if (info->nSkipped++ == info->decimation) {
            info->nSkipped = 0;

            vector<const zcm::LogEvent*> evts;

            zcm::LogEvent le;
            le.timestamp = rbuf->recv_utime;
            le.channel   = channel;
            le.datalen   = rbuf->data_size;
            le.data      = rbuf->data;

            if (!plugins.empty()) {

                int64_t msg_hash;
                __int64_t_decode_array(le.data, 0, 8, &msg_hash, 1);

                for (auto& p : plugins) {
                    vector<const zcm::LogEvent*> pevts =
                        p->transcodeEvent((uint64_t) msg_hash, &le);
                    evts.insert(evts.end(), pevts.begin(), pevts.end());
                }
            }

            if (evts.empty()) evts.push_back(&le);

            for (auto* evt : evts) {
                if (!evt) continue;
                // Must create newChannel in here to handle regex based subscriptions.
                // Ie you cant store the whole channel in BridgeInfo because you
                // don't necessarily know what channel is until you receive a message
                string newChannel = info->prefix + evt->channel;
                info->zcmOut->publish(newChannel, evt->data, evt->datalen);
            }
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

TranscoderPluginDb* Bridge::pluginDb = nullptr;
vector<zcm::TranscoderPlugin*> Bridge::plugins = {};

int main(int argc, char *argv[])
{
    Bridge bridge{};
    if (!bridge.init(argc, argv)) return 1;

    // Register signal handlers
    signal(SIGINT, sighandler);
    signal(SIGQUIT, sighandler);
    signal(SIGTERM, sighandler);

    bridge.run();

    cerr << "Bridge exiting" << endl;

    return 0;
}
