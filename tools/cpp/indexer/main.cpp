#include <iostream>
#include <fstream>
#include <getopt.h>

#include <zcm/zcm-cpp.hpp>

#include "util/json/json.h"

#include "util/Common.hpp"
#include "util/TypeDb.hpp"

using namespace std;

struct Args
{
    string logfile = "";
    string output = "";
    string type_path = "";
    bool debug = false;

    bool parse(int argc, char *argv[])
    {
        // set some defaults
        const char *optstring = "hl:o:t:d";
        struct option long_opts[] = {
            { "help",      no_argument,       0, 'h' },
            { "log",       required_argument, 0, 'l' },
            { "log",       required_argument, 0, 'o' },
            { "type-path", required_argument, 0, 't' },
            { "debug",     no_argument,       0, 'd' },
            { 0, 0, 0, 0 }
        };

        int c;
        while ((c = getopt_long (argc, argv, optstring, long_opts, 0)) >= 0) {
            switch (c) {
                case 'l': logfile   = string(optarg); break;
                case 'o': output    = string(optarg); break;
                case 't': type_path = string(optarg); break;
                case 'd': debug     = true;           break;
                case 'h': default: return false;
            };
        }

        if (logfile == "") {
            cerr << "Please specify logfile input" << endl;
            return false;
        }

        if (output  == "") {
            cerr << "Please specify index file output" << endl;
            return false;
        }

        const char* type_path_env = getenv("ZCM_LOG_INDEXER_PATH");
        if (type_path == "" && type_path_env) type_path = type_path_env;
        if (type_path == "") {
            cerr << "Please specify a zcmtypes.so path either through -t TYPE_PATH "
                    "or through the env var ZCM_LOG_INDEXER_PATH" << endl;
            return false;
        }

        return true;
    }
};

static void usage()
{
    cerr << "usage: zcm-log-indexer [options]" << endl
         << "" << endl
         << "    Load in a log file and write an index json file that" << endl
         << "    allows for faster log indexing." << endl
         << "" << endl
         << "Example:" << endl
         << "    zcm-log-indexer -u udpm://239.255.76.67:7667 -t path/to/zcmtypes.so" << endl
         << "" << endl
         << "Options:" << endl
         << "" << endl
         << "  -h, --help              Shows this help text and exits" << endl
         << "  -l, --log=logfile       Input log to index for fast querying" << endl
         << "  -o, --output=indexfile  Output index file to be used with log" << endl
         << "  -t, --type-path=PATH    Path to a shared library containing the zcmtypes" << endl
         << "  -d, --debug             Run a dry run to ensure proper indexer setup" << endl
         << endl << endl;
}

int main(int argc, char* argv[])
{
    Args args;
    if (!args.parse(argc, argv)) {
        usage();
        return 1;
    }

    zcm::LogFile log(args.logfile, "r");
    if (!log.good()) {
        cerr << "Unable to open logfile: " << args.logfile << endl;
        return 1;
    }

    ofstream output;
    output.open(args.output);
    if (!output.is_open()) {
        cerr << "Unable to open output file: " << args.output << endl;
        log.close();
        return 1;
    }

    TypeDb types(args.type_path, args.debug);

    Json::Value index;

    size_t numEvents = 1;
    off_t offset = 0;
    const zcm::LogEvent* evt;
    while (1) {
        offset = ftello(log.getFilePtr());
        evt = log.readNextEvent();
        if (evt == nullptr) break;

        int64_t msg_hash;
        __int64_t_decode_array(evt->data, 0, 8, &msg_hash, 1);
        const TypeMetadata* md = types.getByHash(msg_hash);
        if (!md) continue;

        index[evt->channel][md->name]["timestamp"].append(to_string(offset));

        numEvents++;
    }

    Json::StreamWriterBuilder builder;
    builder["indentation"] = "";
    std::unique_ptr<Json::StreamWriter> writer(builder.newStreamWriter());
    writer->write(index, &output);
    output.close();

    cout << "Indexed " << numEvents << " events" << endl;
    return 0;
}
