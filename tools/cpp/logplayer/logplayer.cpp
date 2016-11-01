#include <iostream>
#include <sstream>
#include <fstream>
#include <string>
#include <getopt.h>
#include <atomic>
#include <signal.h>
#include <unistd.h>

#include <zcm/zcm-cpp.hpp>

#include "zcm/json/json.h"

#include "util/TimeUtil.hpp"

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
    string jslpFilename = "";
    zcm::Json::Value jslpRoot;

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
        while ((c = getopt_long(argc, argv, "hs:vu:", long_opts, 0)) >= 0) {
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

        jslpFilename = filename + ".jslp";
        ifstream jslpFile { jslpFilename };
        if (jslpFile.good()) {
            zcm::Json::Reader reader;
            if (!reader.parse(jslpFile, jslpRoot, false)) {
                if (verbose) {
                    cerr << "Failed to parse jslp file " << endl;
                    cerr << reader.getFormattedErrorMessages() << endl;
                    return false;
                }
            }
        } else {
            if (verbose) cerr << "No jslp file specified" << endl;
        }

        return true;
    }
};

struct LogPlayer
{
    Args args;
    zcm::LogFile *zcmIn  = nullptr;
    zcm::ZCM     *zcmOut = nullptr;

    string startChan = "";

    LogPlayer() { }

    ~LogPlayer()
    {
        if (zcmIn)  delete zcmIn;
        if (zcmOut) delete zcmOut;
    }

    bool init(int argc, char *argv[])
    {
        if (!args.init(argc, argv))
            return false;

        zcmIn = new zcm::LogFile(args.filename, "r");
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

        if (args.jslpRoot.isMember("START")) {
            if (args.jslpRoot["START"]["mode"] == "channel")
                startChan = args.jslpRoot["START"]["channel"].asString();
        }

        return true;
    }

    void run()
    {
        uint64_t lastMsgUtime = 0;
        uint64_t lastDispatchUtime = 0;
        bool startedPub = false;

        if (startChan == "")
            startedPub = true;

        while (!done) {
            const zcm::LogEvent* le = zcmIn->readNextEvent();
            if (!le) done = true;

            uint64_t now = TimeUtil::utime();

            if (lastMsgUtime == 0)
                lastMsgUtime = le->timestamp;

            if (lastDispatchUtime == 0)
                lastDispatchUtime = now;

            uint64_t localDiff = now - lastDispatchUtime;
            uint64_t logDiff = le->timestamp - lastMsgUtime;
            uint64_t logDiffSpeed = logDiff / args.speed;
            uint64_t diff = logDiffSpeed > localDiff ? logDiffSpeed - localDiff
                                                     : 0;
            if (diff > 0) usleep(diff);

            if (startedPub == false && startChan != "")
                if (le->channel == startChan) startedPub = true;

            auto publish = [&](){
                if (args.verbose)
                    printf("%.3f Channel %-20s size %d\n", le->timestamp / 1e6,
                           le->channel.c_str(), le->datalen);

               zcmOut->publish(le->channel, le->data, le->datalen);
            };

            if (startedPub) {
                if (args.jslpRoot.empty()) {
                    publish();
                } else if (args.jslpRoot.isMember("CHANNEL")) {
                    if (!args.jslpRoot["CHANNEL"].isMember("mode")) {
                        cerr << "Channel filter \"mode\" in jslp file unspecified" << endl;
                        done = true;
                        continue;
                    }

                    if (args.jslpRoot["CHANNEL"]["mode"] == "whitelist") {
                        if (args.jslpRoot["CHANNEL"]["channels"].isArray()) {
                            for (auto val : args.jslpRoot["CHANNEL"]["channels"])
                                if (val == le->channel) publish();
                        } else if (args.jslpRoot["CHANNEL"]["channels"].isMember(le->channel))
                            publish();
                    } else if (args.jslpRoot["CHANNEL"]["mode"] == "blacklist") {
                        if (args.jslpRoot["CHANNEL"]["channels"].isArray()) {
                            bool found = false;
                            for (auto val : args.jslpRoot["CHANNEL"]["channels"])
                                if (val == le->channel) found = true;
                            if (found == false) publish();
                        } else if (!args.jslpRoot["CHANNEL"]["channels"].isMember(le->channel))
                            publish();
                    } else if (args.jslpRoot["CHANNEL"]["mode"] == "specified") {
                        if (!args.jslpRoot["CHANNEL"]["channels"].isMember(le->channel) ||
                             args.jslpRoot["CHANNEL"]["channels"][le->channel] != false) {
                            publish();
                        }
                    } else {
                        cerr << "Channel filter \"mode\" unrecognized: "
                             << args.jslpRoot["CHANNEL"]["mode"] << endl;
                        done = true;
                        continue;
                    }
                }
            }

            lastDispatchUtime = TimeUtil::utime();
            lastMsgUtime = le->timestamp;
        }
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
