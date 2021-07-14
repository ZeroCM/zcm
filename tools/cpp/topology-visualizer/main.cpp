#include <iostream>
#include <fstream>
#include <getopt.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unordered_set>
#include <unistd.h>

#include "zcm/json/json.h"
#include "zcm/util/debug.h"

#include "util/StringUtil.hpp"
#include "util/TypeDb.hpp"

using namespace std;

struct Args
{
    string topology_dir = "";
    string output       = "";
    string type_path    = "";
    bool debug          = false;

    bool parse(int argc, char *argv[])
    {
        // set some defaults
        const char *optstring = "d:o:t:h";
        struct option long_opts[] = {
            { "topology-dir",  required_argument, 0, 'd' },
            { "output",        required_argument, 0, 'o' },
            { "type-path",     required_argument, 0, 't' },
            { "debug",         no_argument,       0,  0  },
            { "help",          no_argument,       0, 'h' },
            { 0, 0, 0, 0 }
        };

        int c;
        int option_index;
        while ((c = getopt_long(argc, argv, optstring, long_opts, &option_index)) >= 0) {
            switch (c) {
                case 'd': topology_dir = string(optarg); break;
                case 'o': output       = string(optarg); break;
                case 't': type_path    = string(optarg); break;
                case  0: {
                    string longopt = string(long_opts[option_index].name);
                    if (longopt == "debug") debug = true;
                    break;
                }
                case 'h': default: usage(); return false;
            };
        }

        if (topology_dir == "") {
            cerr << "Please specify a topology directory to "
                 << "read topology json files from" << endl;
            return false;
        }
        if (topology_dir.back() != '/') topology_dir += "/";

        if (output  == "") {
            cerr << "Please specify dotvis file output" << endl;
            return false;
        }

        if (type_path == "") {
            cerr << "Please specify a zcmtypes.so path" << endl;
            return false;
        }

        return true;
    }

    void usage()
    {
        cout << "usage: zcm-topology-visualizer [options]" << endl
             << "" << endl
             << "    Scrape through a topology directory full of topology json files" << endl
             << "    and generate a single dotvis file output that visualizes all of the" << endl
             << "    different messages received and sent on different channels" << endl
             << "" << endl
             << "Example:" << endl
             << "    zcm-topology-visualizer -d /tmp/zcm_topology -o /tmp/topology.dot -t path/to/zcmtypes.so" << endl
             << "" << endl
             << "Options:" << endl
             << "" << endl
             << "  -h, --help              Shows this help text and exits" << endl
             << "  -d, --topology-dir=dir  Directory to scrape for topology json files" << endl
             << "  -o, --output=file       Output dotvis file" << endl
             << "  -t, --type-path=path    Path to shared library containing zcmtypes" << endl
             << "      --debug             Run a dry run to ensure proper indexer setup" << endl
             << endl << endl;
    }
};

int walkFiles(const string& directory, vector<string>& files)
{
    DIR *dp = opendir(directory.c_str());
    if (dp == nullptr) {
        cerr << "Unable to open directory: " << directory << endl;
        return 1;
    }

    struct dirent *entry = nullptr;
    while ((entry = readdir(dp))) {
        string name(entry->d_name);
        if (name == "." || name == "..") continue;
        if (!StringUtil::endswith(name, ".json")) continue;

        struct stat info;
        string fullpath = directory + name;
        int ret = stat(fullpath.c_str(), &info);
        if (ret != 0) {
            cerr << "Unable to open file: " << directory << entry->d_name << endl;
            perror("stat");
            continue;
        }
        if (!S_ISREG(info.st_mode)) continue;

        files.push_back(fullpath);
    }

    if (closedir(dp) != 0) return 1;

    return 0;
}

void buildIndex(zcm::Json::Value& index, const vector<string>& files)
{
    for (const auto& f : files) {
        ifstream infile{ f };
        if (!infile.good()) {
            cerr << "Unable to open file: " << f << endl;
            continue;
        }

        zcm::Json::Value root;
        zcm::Json::Reader reader;
        if (!reader.parse(infile, root, false)) {
            cerr << "Failed to parse json file: " << f << endl;
            cerr << reader.getFormattedErrorMessages() << endl;
            continue;
        }

        index[root["name"].asString()] = root;
    }
}

// RRR: can you use const refs instead of by-value for the other 2 args?
int writeOutput(zcm::Json::Value index, const string& outpath, TypeDb types)
{
    ofstream output{ outpath };
    if (!output.good()) {
        cerr << "Unable to open output file: " << outpath << endl;
        return 1;
    }
    output << "digraph arch {" << endl;
    output << endl;

    unordered_map<string, string> names;
    unordered_map<string, string> channels;
    for (auto n : index.getMemberNames()) {
        if (names.count(n) == 0) {
            size_t i = names.size();
            names[n] = string("name") + to_string(i);
        }
        for (auto chan : index[n]["publishes"].getMemberNames()) {
            if (channels.count(chan) > 0) continue;
            size_t i = channels.size();
            channels[chan] = string("channel") + to_string(i);
        }
        for (auto chan : index[n]["subscribes"].getMemberNames()) {
            if (channels.count(chan) > 0) continue;
            size_t i = channels.size();
            channels[chan] = string("channel") + to_string(i);
        }
    }

    for (auto c : channels) {
        output << "  " << c.second << " [label=" << zcm::Json::Value(c.first) << " shape=rectangle]" << endl;
    }
    for (auto n : names) {
        output << "  " << n.second << " [label=" << zcm::Json::Value(n.first) << " shape=oval]" << endl;
    }


    for (auto n : index.getMemberNames()) {
        for (auto channel : index[n]["publishes"].getMemberNames()) {
            auto s = index[n]["publishes"][channel];
            vector<string> typeNames;
            for (auto t : s) {
                int64_t hashBE = stoll(t["BE"].asString());
                int64_t hashLE = stoll(t["LE"].asString());
                const TypeMetadata* md = types.getByHash(hashBE);
                if (!md) md = types.getByHash(hashLE);
                if (!md) {
                    cerr << "Unable to find matching type for hash pair: " << t << endl;
                    continue;
                }
                typeNames.push_back(md->name);
            }
            output << "  " << names[n] << " -> " << channels[channel] << " [label=\"";
            for (auto t : typeNames) output << t << " ";
            output << "\" color=blue]" << endl;
        }
        for (auto channel : index[n]["subscribes"].getMemberNames()) {
            auto s = index[n]["subscribes"][channel];
            vector<string> typeNames;
            for (auto t : s) {
                int64_t hashBE = stoll(t["BE"].asString());
                int64_t hashLE = stoll(t["LE"].asString());
                const TypeMetadata* md = types.getByHash(hashBE);
                if (!md) md = types.getByHash(hashLE);
                if (!md) {
                    cerr << "Unable to find matching type for hash pair: " << t << endl;
                    continue;
                }
                typeNames.push_back(md->name);
            }
            output << "  " << channels[channel] << " -> " << names[n] << " [label=\"";
            for (auto t : typeNames) output << t << " ";
            output << "\" color=red]" << endl;
        }
    }

    output << "}" << endl;

    output.close();

    return 0;
}

int main(int argc, char *argv[])
{
    Args args;
    if (!args.parse(argc, argv)) return 1;

    TypeDb types(args.type_path, args.debug);

    if (!types.good()) return 1;

    if (args.debug) return 0;

    vector<string> files;
    int ret = walkFiles(args.topology_dir, files);
    if (ret != 0) return ret;

    zcm::Json::Value index;
    buildIndex(index, files);

    ret = writeOutput(index, args.output, types);
    if (ret != 0) return ret;

    // RRR: are you running xdot for them? That seems kinda weird.
    //      At least make it optional?
    ret = execlp("xdot", "xdot", args.output.c_str(), nullptr);
    if (ret != 0) {
        cout << "Successfully wrote file to " << args.output << endl;
        cout << "Run `xdot " << args.output << "` to view output" << endl;
    }

    return 0;
}
