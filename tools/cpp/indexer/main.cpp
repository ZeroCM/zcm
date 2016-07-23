#include <iostream>
#include <fstream>
#include <getopt.h>
#include <algorithm>
#include <memory>

#include <zcm/zcm-cpp.hpp>

#include "zcm/json/json.h"

#include "zcm/util/TypeDb.hpp"

#include "IndexerPluginDb.hpp"

using namespace std;

struct Args
{
    string logfile     = "";
    string output      = "";
    string plugin_path = "";
    string type_path   = "";
    bool readable      = false;
    bool debug         = false;

    bool parse(int argc, char *argv[])
    {
        // set some defaults
        const char *optstring = "hl:o:p:t:rd";
        struct option long_opts[] = {
            { "help",        no_argument,       0, 'h' },
            { "log",         required_argument, 0, 'l' },
            { "log",         required_argument, 0, 'o' },
            { "plugin-path", required_argument, 0, 'p' },
            { "type-path",   required_argument, 0, 't' },
            { "readable",    no_argument,       0, 'r' },
            { "debug",       no_argument,       0, 'd' },
            { 0, 0, 0, 0 }
        };

        int c;
        while ((c = getopt_long (argc, argv, optstring, long_opts, 0)) >= 0) {
            switch (c) {
                case 'l': logfile     = string(optarg); break;
                case 'o': output      = string(optarg); break;
                case 'p': plugin_path = string(optarg); break;
                case 't': type_path   = string(optarg); break;
                case 'r': readable    = true;           break;
                case 'd': debug       = true;           break;
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

        const char* type_path_env = getenv("ZCM_LOG_INDEXER_ZCMTYPES_PATH");
        if (type_path == "" && type_path_env) type_path = type_path_env;
        if (type_path == "") {
            cerr << "Please specify a zcmtypes.so path either through -t TYPE_PATH "
                    "or through the env var ZCM_LOG_INDEXER_ZCMTYPES_PATH" << endl;
            return false;
        }

        const char* plugin_path_env = getenv("ZCM_LOG_INDEXER_PLUGIN_PATH");
        if (plugin_path == "" && plugin_path_env) plugin_path = plugin_path_env;
        if (plugin_path == "") cerr << "Running with default timestamp indexer plugin" << endl;

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
         << "    zcm-log-indexer -l zcm.log -o index.dbz -t path/to/zcmtypes.so" << endl
         << "" << endl
         << "Options:" << endl
         << "" << endl
         << "  -h, --help              Shows this help text and exits" << endl
         << "  -l, --log=logfile       Input log to index for fast querying" << endl
         << "  -o, --output=indexfile  Output index file to be used with log" << endl
         << "  -p, --plugin-path=path  Path to shared library containing indexer plugins" << endl
         << "  -t, --type-path=path    Path to shared library containing the zcmtypes" << endl
         << "  -r, --readable          Don't minify the output index file. " << endl
         << "                          Leave it human readable" << endl
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
    fseeko(log.getFilePtr(), 0, SEEK_END);
    off_t logSize = ftello(log.getFilePtr());

    ofstream output;
    output.open(args.output);
    if (!output.is_open()) {
        cerr << "Unable to open output file: " << args.output << endl;
        log.close();
        return 1;
    }

    vector<const zcm::IndexerPlugin*> plugins;

    zcm::IndexerPlugin* defaultPlugin = new zcm::IndexerPlugin();
    plugins.push_back(defaultPlugin);

    IndexerPluginDb* pluginDb = nullptr;
    // Load plugins from path if specified
    if (args.plugin_path != "") {
        pluginDb = new IndexerPluginDb(args.plugin_path, args.debug);
        vector<const zcm::IndexerPlugin*> dbPlugins = pluginDb->getPlugins();
        plugins.insert(plugins.end(), dbPlugins.begin(), dbPlugins.end());
    }

    auto buildPluginGroups = [] (vector<const zcm::IndexerPlugin*> plugins) {
        vector<vector<const zcm::IndexerPlugin*>> groups;
        vector<const zcm::IndexerPlugin*> lastLoop = plugins;
        while (!plugins.empty()) {
            groups.resize(groups.size() + 1);
            for (auto p = plugins.begin(); p != plugins.end();) {
                vector<string> deps = (*p)->dependencies();

                bool skipUntilLater = false;
                for (auto* dep : plugins) {
                    if (find(deps.begin(), deps.end(), dep->name()) != deps.end()) {
                        skipUntilLater = true;
                        break;
                    }
                }
                for (auto* dep : groups.back()) {
                    if (find(deps.begin(), deps.end(), dep->name()) != deps.end()) {
                        skipUntilLater = true;
                        break;
                    }
                }
                if (!skipUntilLater) {
                    groups.back().push_back(*p);
                    p = plugins.erase(p);
                    if (p == plugins.end()) break;
                } else {
                    ++p;
                }
            }
            if (plugins == lastLoop) {
                cerr << "Unable to resolve all plugin dependencies" << endl;
                exit(1);
            }
            lastLoop = plugins;
        }
        return groups;
    };

    vector<vector<const zcm::IndexerPlugin*>> pluginGroups;
    pluginGroups = buildPluginGroups(plugins);
    if (pluginGroups.size() > 1) {
        cout << "Identified " << pluginGroups.size() << " indexer plugin groups" << endl
             << "Running through log " << pluginGroups.size()
             << " times to satisfy dependencies" << endl;
    }

    TypeDb types(args.type_path, args.debug);

    Json::Value index;

    struct SortingInfo {
        vector<off_t> offsets;
        const zcm::IndexerPlugin* plugin;
    };

    unordered_map<Json::Value*, SortingInfo> needSorting;

    size_t numEvents = 1;
    const zcm::LogEvent* evt;
    for (size_t i = 0; i < pluginGroups.size(); ++i) {
        if (pluginGroups.size() != 1) cout << "Plugin group " << (i + 1) << endl;
        off_t offset = 0;
        fseeko(log.getFilePtr(), 0, SEEK_SET);
        while (1) {
            offset = ftello(log.getFilePtr());

            static int lastPrintPercent = 0;
            int percent = (100.0 * offset / logSize) * 100;
            if (percent != lastPrintPercent) {
                cout << "\r" << "Percent Complete: " << (percent / 100) << flush;
                lastPrintPercent = percent;
            }

            evt = log.readNextEvent();
            if (evt == nullptr) break;

            int64_t msg_hash;
            __int64_t_decode_array(evt->data, 0, 8, &msg_hash, 1);
            const TypeMetadata* md = types.getByHash(msg_hash);
            if (!md) continue;

            for (auto* p : pluginGroups[i]) {
                assert(p);
                vector<string> indexName = p->includeInIndex(evt->channel, md->name,
                                                             (uint64_t) msg_hash,
                                                             evt->data, evt->datalen);
                if (indexName.size() == 0) continue;

                Json::Value* currIndex = &index[p->name()];
                for (size_t j = 0; j < indexName.size(); ++j) {
                    currIndex = &(*currIndex)[indexName[j]];
                }
                if (needSorting.count(currIndex))
                    needSorting[currIndex].offsets.push_back(offset);
                else
                    needSorting[currIndex] = SortingInfo{{offset}, p};
                numEvents++;
            }

        }

        cout << endl;

        for (auto& s : needSorting) {
            cout << "sorting " << s.second.plugin->name() << endl;
            auto comparator = [&](off_t a, off_t b) {
                if (a < 0 || b < 0 || a > logSize || b > logSize) {
                    cerr << "Sorting has failed. "
                         << "Your sorting function is probably broken. "
                         << "Aborting." << endl;
                    exit(1);
                }
                return s.second.plugin->lessThan(a, b, log, index);
            };
            if (s.second.plugin->sorted())
                std::sort(s.second.offsets.begin(), s.second.offsets.end(), comparator);
            for (auto val : s.second.offsets) (*s.first).append(to_string(val));
        }
        needSorting.clear();

    }

    delete defaultPlugin;
    defaultPlugin = nullptr;
    if (pluginDb) {
        delete pluginDb;
        pluginDb = nullptr;
    }

    Json::StreamWriterBuilder builder;
    builder["indentation"] = args.readable ? "    " : "";
    std::unique_ptr<Json::StreamWriter> writer(builder.newStreamWriter());
    writer->write(index, &output);
    output << endl;
    output.close();

    cout << "Indexed " << numEvents << " events" << endl;
    return 0;
}
