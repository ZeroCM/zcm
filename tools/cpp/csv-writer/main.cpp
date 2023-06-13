#include <fstream>
#include <getopt.h>
#include <iomanip>
#include <iostream>
#include <limits>
#include <memory>
#include <vector>

#include <zcm/zcm-cpp.hpp>

#include "util/Introspection.hpp"
#include "util/StringUtil.hpp"
#include "util/TypeDb.hpp"

using namespace std;

static streamsize defaultPrecision{6};

struct Field
{
    string header;  // x
    string channel; // POSE
    string field;   // position.0

    string fullpath; // POSE.position.0

    mutable double initialValue = 0;

    streamsize precision = defaultPrecision;

    Field(const string& header, const string& channel, const string& field) :
        header(header), channel(channel), field(field), fullpath(channel + "." + field) {}
};

struct Args
{
    string inlog    = "";
    string outfile  = "";
    string channels = "";
    bool debug      = false;

    unique_ptr<zcm::TypeDb> types;

    vector<Field> fields;

    bool parse(int argc, char *argv[])
    {
        // set some defaults
        const char *optstring = "i:o:t:f:bp:dh";
        struct option long_opts[] = {
            { "input",      required_argument, 0, 'i' },
            { "output",     required_argument, 0, 'o' },
            { "type-path",  required_argument, 0, 't' },
            { "field",      required_argument, 0, 'f' },
            { "base-field", no_argument,       0, 'b' },
            { "precision",  required_argument, 0, 'p' },
            { "debug",      no_argument,       0, 'd' },
            { "help",       no_argument,       0, 'h' },
            { 0, 0, 0, 0 }
        };

        string typepath = "";
        vector<string> fieldstrings;

        int c;
        while ((c = getopt_long (argc, argv, optstring, long_opts, 0)) >= 0) {
            switch (c) {
                case 'i': inlog = string(optarg); break;
                case 'o': outfile = string(optarg); break;
                case 't': typepath = string(optarg); break;
                case 'f': {
                    auto parts = StringUtil::split(optarg, ':');
                    if (parts.size() != 3) {
                        cerr << "Malformed field: " << optarg << endl;
                        return false;
                    }
                    fields.emplace_back(parts[0], parts[1], parts[2]);
                    break;
                }
                case 'b': {
                    if (fields.empty()) {
                        cerr << "No field to turn into a base field. Specify -b after -f" << endl;
                        return false;
                    }
                    fields.back().initialValue = numeric_limits<double>::max(); break;
                    break;
                }
                case 'p': {
                    if (fields.empty()) {
                        cerr << "Setting default precision: " << optarg << endl;
                        defaultPrecision = atof(optarg);
                        if (defaultPrecision == 0) {
                            cerr << "Specify nonzero default precision" << endl;
                            return false;
                        }
                    } else {
                        fields.back().precision = atoi(optarg); break;
                        if (fields.back().precision == 0) {
                            cerr << "Specify nonzero precision" << endl;
                            return false;
                        }
                    }
                    break;
                }
                case 'd': debug = true; break;
                case 'h':
                default: usage(); return false;
            };
        }

        if (inlog == "") {
            cerr << "Please specify logfile input" << endl;
            return false;
        }

        if (outfile == "") {
            cerr << "Please specify output file path" << endl;
            return false;
        }

        if (typepath == "") {
            cerr << "Please specify type library path" << endl;
            return false;
        }

        types.reset(new zcm::TypeDb(typepath, debug));
        if (!types->good()) {
            cerr << "Unable to load types library: " << typepath << endl;
            return false;
        }

        if (debug) {
            cout << "Fields: ";
            if (fields.empty()) cout << "All";
            cout << endl;
            for (const auto& f : fields) {
                cout << "  \"" << f.header << "\" (on "
                     << f.channel << "): " << f.field << endl;
            }
        }

        return true;
    }

    void usage()
    {
        cout << "usage: zcm-csv-writer [options]" << endl
             << "" << endl
             << "    Convert zcm traffic (from log or realtime) to csv output" << endl
             << endl
             << "Examples:" << endl
             << "    # Convert all fields in all messages on all channels to a" << endl
             << "    # csv output and print to screen" << endl
             << "    zcm-csv-writer -i in.log -t types.so" << endl
             << endl
             << "    # Convert the first element of position in messages " << endl
             << "    # received on channel POSE to a csv and write to a file" << endl
             << "    zcm-log-filter -i in.log -o out.csv -t types.so "
                                   "-f x:POSE:position.0" << endl
             << endl
             << "Options:" << endl
             << endl
             << "  -h, --help             Shows this help text and exits" << endl
             << "  -i, --input=logfile    Input log to filter" << endl
             << "  -o, --output=logfile   Output filtered log file" << endl
             << "  -t, --type-path=path   Path to shared library containing zcmtypes" << endl
             << "  -f, --field=<field>    A config-style description of a field in a channel/message" << endl
             << "                         Formatted as: HEADER:CHANNEL_NAME:FIELD" << endl
             << "                         Where FIELD can be nested fields using \".\" as the" << endl
             << "                         nested field accessor. Specify in the order " << endl
             << "                         you want the columns to appear in output" << endl
             << "                         csv data" << endl
             << "  -b, --base-field       Put this after your field arg to subtract" << endl
             << "                         the initial value of that field from all records" << endl
             << "  -p, --precision        Put this after your field arg to specify" << endl
             << "                         the print precision of the field" << endl
             << "                         Defaults to: " << defaultPrecision << endl
             << "  -d, --debug            Run a dry run to ensure proper setup" << endl
             << endl << endl;
    }
};

static void writeHeaders(const Args& args, ostream& os)
{
    if (args.fields.empty()) {
        os << "field,value" << endl;
        return;
    }
    for (size_t i = 0; i < args.fields.size(); ++i) {
        const auto& f = args.fields[i];
        os << f.header;
        if (i < args.fields.size() - 1) os << ",";
    }
    os << endl;
}

struct ProcessUsr
{
    vector<pair<string, double>>& numerics;
    vector<pair<string, string>>& strings;
};
static void processScalar(const string& name, zcm_field_type_t type,
                          const void* data, void* usr)
{
    ProcessUsr *v = (ProcessUsr*) usr;
    switch (type) {
        case ZCM_FIELD_INT8_T: v->numerics.emplace_back(name, *((int8_t*)data)); break;
        case ZCM_FIELD_INT16_T: v->numerics.emplace_back(name, *((int16_t*)data)); break;
        case ZCM_FIELD_INT32_T: v->numerics.emplace_back(name, *((int32_t*)data)); break;
        case ZCM_FIELD_INT64_T: v->numerics.emplace_back(name, *((int64_t*)data)); break;
        case ZCM_FIELD_BYTE: v->numerics.emplace_back(name, *((uint8_t*)data)); break;
        case ZCM_FIELD_FLOAT: v->numerics.emplace_back(name, *((float*)data)); break;
        case ZCM_FIELD_DOUBLE: v->numerics.emplace_back(name, *((double*)data)); break;
        case ZCM_FIELD_BOOLEAN: v->numerics.emplace_back(name, *((bool*)data)); break;
        case ZCM_FIELD_STRING: v->strings.emplace_back(name, string((const char*)data)); break;
        case ZCM_FIELD_USER_TYPE: assert(false && "Should not be possble");
    }
}

static void handleEvent(const Args& args,
                        int64_t timestamp, const string& channel,
                        uint8_t* data, int32_t datalen,
                        ostream& os)
{
    if (!args.fields.empty()) {
        bool subscribed = false;
        for (const auto& f : args.fields) {
            if (f.channel == channel) {
                subscribed = true;
                break;
            }
        }
        if (!subscribed) return;
    }

    vector<pair<string, double>> numerics;
    vector<pair<string, string>> strings;
    ProcessUsr usr = { numerics, strings };
    zcm::Introspection::processEncodedType(channel,
                                           data, datalen,
                                           ".",
                                           *args.types.get(),
                                           processScalar, &usr);

    if (!args.fields.empty()) {
        for (size_t i = 0; i < args.fields.size(); ++i) {
            const auto& f = args.fields[i];
            for (const auto& n : numerics) {
                if (n.first == f.fullpath) {
                    if (f.initialValue == numeric_limits<double>::max())
                        f.initialValue = n.second;
                    os << setprecision(f.precision) << n.second - f.initialValue;
                    break;
                }
            }
            for (const auto& s : strings) {
                if (s.first == f.fullpath) {
                    os << s.second;
                    break;
                }
            }
            if (i < args.fields.size() - 1) os << ",";
        }
        os << endl;
    } else {
        for (auto& n : numerics) os << setprecision(defaultPrecision)
                                    << n.first << "," << n.second << endl;
        for (auto& s : strings) os << s.first << "," << s.second << endl;
    }
}

static int processInputLog(const Args& args, ostream& os)
{
    zcm::LogFile inlog(args.inlog, "r");
    if (!inlog.good()) {
        cerr << "Unable to open input zcm log: " << args.inlog << endl;
        return 1;
    }

    auto processLog = [&inlog](function<void(const zcm::LogEvent* evt)> processEvent) {
        const zcm::LogEvent* evt;
        off64_t offset;
        static int lastPrintPercent = 0;

        fseeko(inlog.getFilePtr(), 0, SEEK_END);
        off64_t logSize = ftello(inlog.getFilePtr());
        fseeko(inlog.getFilePtr(), 0, SEEK_SET);

        while (1) {
            offset = ftello(inlog.getFilePtr());

            int percent = 100.0 * offset / (logSize == 0 ? 1 : logSize);
            if (percent != lastPrintPercent) {
                cout << "\r" << "Percent Complete: " << percent << flush;
                lastPrintPercent = percent;
            }

            evt = inlog.readNextEvent();
            if (evt == nullptr) break;

            processEvent(evt);
        }
        if (lastPrintPercent != 100) cout << "\r" << "Percent Complete: 100" << flush;
        cout << endl;
    };

    processLog([&args, &os](const zcm::LogEvent* evt){
        handleEvent(args, evt->timestamp, evt->channel, evt->data, evt->datalen, os);
    });

    inlog.close();

    return 0;
}

int main(int argc, char* argv[])
{
    Args args;
    bool success = args.parse(argc, argv);
    if (!success) return 1;

    if (args.debug) return 0;

    ofstream os(args.outfile);
    if (!os.good()) {
        cerr << "Unable to open output file: " << args.outfile << endl;
        return 1;
    }

    writeHeaders(args, os);

    int ret = 0;

    if (!args.inlog.empty()) ret = processInputLog(args, os);

    os.close();

    return ret;
}
