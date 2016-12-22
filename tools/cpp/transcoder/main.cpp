#include <iostream>
#include <fstream>
#include <getopt.h>
#include <algorithm>
#include <memory>

#include <zcm/zcm-cpp.hpp>

#include "zcm/json/json.h"

#include "util/TypeDb.hpp"

#include "TranscoderPluginDb.hpp"

using namespace std;

struct Args
{
    string inlog       = "";
    string outlog      = "";
    string plugin_path = "";
    string type_path   = "";
    bool debug         = false;

    bool parse(int argc, char *argv[])
    {
        // set some defaults
        const char *optstring = "l:o:p:t:dh";
        struct option long_opts[] = {
            { "log",         required_argument, 0, 'l' },
            { "output",      required_argument, 0, 'o' },
            { "plugin-path", required_argument, 0, 'p' },
            { "type-path",   required_argument, 0, 't' },
            { "debug",       no_argument,       0, 'd' },
            { "help",        no_argument,       0, 'h' },
            { 0, 0, 0, 0 }
        };

        int c;
        while ((c = getopt_long (argc, argv, optstring, long_opts, 0)) >= 0) {
            switch (c) {
                case 'l': inlog       = string(optarg); break;
                case 'o': outlog      = string(optarg); break;
                case 'p': plugin_path = string(optarg); break;
                case 't': type_path   = string(optarg); break;
                case 'd': debug       = true;           break;
                case 'h': usage();                      return true;
                default:                                return false;
            };
        }

        if (inlog == "") {
            cerr << "Please specify logfile input" << endl;
            return false;
        }

        if (outlog  == "") {
            cerr << "Please specify log file output" << endl;
            return false;
        }

        const char* type_path_env = getenv("ZCM_LOG_TRANSCODER_ZCMTYPES_PATH");
        if (type_path == "" && type_path_env) type_path = type_path_env;
        if (type_path == "") {
            cerr << "Please specify a zcmtypes.so path either through -t TYPE_PATH "
                    "or through the env var ZCM_LOG_INDEXER_ZCMTYPES_PATH" << endl;
            return false;
        }

        const char* plugin_path_env = getenv("ZCM_LOG_TRANSCODER_PLUGINS_PATH");
        if (plugin_path == "" && plugin_path_env) plugin_path = plugin_path_env;
        if (plugin_path == "") {
            cerr << "No plugin specified. Nothing to do" << endl;
            return false;
        }

        return true;
    }

    void usage()
    {
        cerr << "usage: zcm-log-transcoder [options]" << endl
             << "" << endl
             << "    Convert messages in one log file from one zcm type to another" << endl
             << "" << endl
             << "Example:" << endl
             << "    zcm-log-transcoder -l zcm.log -o zcmout.log -p path/to/plugin.so" << endl
             << "" << endl
             << "Options:" << endl
             << "" << endl
             << "  -h, --help              Shows this help text and exits" << endl
             << "  -l, --log=logfile       Input log to convert" << endl
             << "  -o, --output=logfile    Output converted log file" << endl
             << "  -p, --plugin-path=path  Path to shared library containing transcoder plugins" << endl
             << "                          Can also be specified via the environment variable" << endl
             << "                          ZCM_LOG_TRANSCODER_PLUGINS_PATH" << endl
             << "  -t, --type-path=path    Path to shared library containing the zcmtypes" << endl
             << "                          Can also be specified via the environment variable" << endl
             << "                          ZCM_LOG_TRANSCODER_ZCMTYPES_PATH" << endl
             << "  -d, --debug             Run a dry run to ensure proper transcoder setup" << endl
             << endl << endl;
    }
};

int main(int argc, char* argv[])
{
    Args args;
    if (!args.parse(argc, argv)) {
        args.usage();
        return 1;
    }

    zcm::LogFile inlog(args.inlog, "r");
    if (!inlog.good()) {
        cerr << "Unable to open input zcm log: " << args.inlog << endl;
        return 1;
    }
    fseeko(inlog.getFilePtr(), 0, SEEK_END);
    off64_t logSize = ftello(inlog.getFilePtr());

    zcm::LogFile outlog(args.outlog, "w");
    if (!outlog.good()) {
        cerr << "Unable to open output zcm log: " << args.inlog << endl;
        return 1;
    }


    vector<zcm::TranscoderPlugin*> plugins;

    TranscoderPluginDb pluginDb(args.plugin_path, args.debug);
    // Load plugins from path if specified
    if (args.plugin_path != "") {
        vector<const zcm::TranscoderPlugin*> dbPlugins = pluginDb.getPlugins();
        if (dbPlugins.empty()) {
            cerr << "Couldn't find any plugins. Aborting." << endl;
            return 1;
        }

        // casting away constness. Don't mess up.
        for (auto dbp : dbPlugins) plugins.push_back((zcm::TranscoderPlugin*) dbp);
    }

    TypeDb types(args.type_path, args.debug);

    if (args.debug) return 0;

    size_t numInEvents = 0, numOutEvents = 0;
    const zcm::LogEvent* evt;
    off64_t offset;
    fseeko(inlog.getFilePtr(), 0, SEEK_SET);

    while (1) {
        offset = ftello(inlog.getFilePtr());

        static int lastPrintPercent = 0;
        int percent = (100.0 * offset / logSize) * 100;
        if (percent != lastPrintPercent) {
            cout << "\r" << "Percent Complete: " << (percent / 100) << flush;
            lastPrintPercent = percent;
        }

        evt = inlog.readNextEvent();
        if (evt == nullptr) break;

        int64_t msg_hash;
        __int64_t_decode_array(evt->data, 0, 8, &msg_hash, 1);
        const TypeMetadata* md = types.getByHash(msg_hash);
        if (!md) continue;

        for (auto& p : plugins) {
            vector<const zcm::LogEvent*> evts = p->transcodeEvent((uint64_t) msg_hash, evt);
            for (auto& evt : evts) {
                outlog.writeEvent(evt);
                numOutEvents++;
            }
        }

        numInEvents++;
    }
    cout << endl;

    inlog.close();
    outlog.close();

    cout << "Transcoded " << numInEvents << " events into " << numOutEvents << " events" << endl;
    return 0;
}
