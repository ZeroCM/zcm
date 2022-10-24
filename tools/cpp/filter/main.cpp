#include <iostream>
#include <fstream>
#include <getopt.h>
#include <memory>
#include <vector>
#include <unordered_set>

#include <zcm/zcm-cpp.hpp>

#include "util/StringUtil.hpp"
#include "util/TypeDb.hpp"

using namespace std;

struct Args
{
    string inlog  = "";
    string outlog = "";
    bool debug    = false;

    unique_ptr<TypeDb> types;

    enum ConditionSourceType
    {
        Seconds,
        Field,
        NumSourceTypes,
    };

    class FieldAccessor
    {
      public:
        const TypeMetadata* md = nullptr;
        int fIdx = -1;

        FieldAccessor(const string& msgtype, const string& field, const TypeDb* types)
        {
            md = types->getByName(StringUtil::dotsToUnderscores(msgtype));
            if (!md) return;

            auto info = md->info;

            uint8_t *buf = new uint8_t[info->struct_size()];

            size_t numFields = (size_t)info->num_fields();
            for (size_t i = 0; i < numFields; ++i) {
                zcm_field_t f;
                assert(info->get_field(buf, i, &f) >= 0);

                if (string(f.name) == field) {
                    fIdx = i;
                    break;
                }
            }

            delete[] buf;
        }

        bool good() const
        {
            return md && fIdx >= 0;
        }

        bool get(const void* data, zcm_field_t& f) const
        {
            if (md->info->get_field(data, fIdx, &f)) return false;
            return true;
        }
    };

    class Condition
    {
        ConditionSourceType type = ConditionSourceType::NumSourceTypes;

        // Valid only for type == ConditionSourceType::Field
        string channel;
        string msgtype;
        string msgfield;
        vector<FieldAccessor> accessors;

        bool compBool;
        bool inverse;
        bool boolIsActive(bool val) const
        {
            return inverse ? !val : val;
        }

        double number;
        bool lessThan;
        bool numberIsActive(double val) const
        {
            return lessThan ? val < number : val >= number;
        }

        bool isConfigured = false;

        struct FlaggedBool
        {
            bool isValid;
            bool val;
        };

        struct FlaggedNumber
        {
            bool isValid;
            double val;
        };

        bool getField(const zcm::LogEvent* le, zcm_field_t& f) const
        {
            assert(!accessors.empty());
            auto info = accessors.front().md->info;
            uint8_t* buf = new uint8_t[(size_t)info->struct_size()];
            if (info->decode(le->data, 0, le->datalen, buf) != le->datalen) {
                delete[] buf;
                return false;
            }
            void* lastData = (void*)buf;
            for (auto& a : accessors) {
                if (!a.get(lastData, f)) {
                    delete[] buf;
                    return false;
                }
                lastData = f.data;
            }
            delete[] buf;
            return true;
        }

        FlaggedBool extractBool(const zcm::LogEvent* le) const
        {
            if (le->channel != channel) return { false, false };

            zcm_field_t f;
            if (!getField(le, f)) return { false, false };
            bool ret;
            switch (f.type) {
                case ZCM_FIELD_INT8_T:
                    ret = *((int8_t*)f.data);
                    break;
                case ZCM_FIELD_INT16_T:
                    ret = *((int16_t*)f.data);
                    break;
                case ZCM_FIELD_INT32_T:
                    ret = *((int32_t*)f.data);
                    break;
                case ZCM_FIELD_INT64_T:
                    ret = *((int64_t*)f.data);
                    break;
                case ZCM_FIELD_BOOLEAN:
                    ret = *((bool*)f.data);
                    break;
                default:
                    return { false, false };
            }

            return { true, ret };
        }

        FlaggedNumber extractNumber(const zcm::LogEvent* le) const
        {
            if (le->channel != channel) return { false, 0.0 };

            zcm_field_t f;
            if (!getField(le, f)) return { false, 0.0 };
            switch (f.type) {
                case ZCM_FIELD_INT8_T:
                    return { true, (double)*((int8_t*)f.data) };
                case ZCM_FIELD_INT16_T:
                    return { true, (double)*((int16_t*)f.data) };
                case ZCM_FIELD_INT32_T:
                    return { true, (double)*((int32_t*)f.data) };
                case ZCM_FIELD_INT64_T:
                    return { true, (double)*((int64_t*)f.data) };
                case ZCM_FIELD_FLOAT:
                    return { true, (double)*((float*)f.data) };
                case ZCM_FIELD_DOUBLE:
                    return { true,         *((double*)f.data) };
                default:
                    return { false, 0.0 };
            }
        }

      public:
        Condition() {}

        virtual ~Condition() {}

        virtual bool isFullySpecified() const { return isConfigured; }

        virtual bool setSeconds()
        {
            if (type != ConditionSourceType::NumSourceTypes) return false;
            type = ConditionSourceType::Seconds;
            return true;
        }

        virtual bool setField(const string& field, const TypeDb* types)
        {
            if (type != ConditionSourceType::NumSourceTypes) return false;
            type = ConditionSourceType::Field;

            auto parts = StringUtil::split(field, ':');
            if (parts.size() != 3) return false;

            channel = parts[0];
            msgtype = parts[1];
            msgfield = parts[2];
            auto fieldParts = StringUtil::split(msgfield, '.');

            for (size_t i = 0; i < fieldParts.size(); ++i) {
                accessors.emplace_back(i == 0 ? msgtype : fieldParts[i - 1],
                                       fieldParts[i], types);
                if (!accessors.back().good()) return false;
            }

            return true;
        }

        virtual bool setCompound(bool _and) { return false; }

        virtual bool setCompBool(bool _inverse)
        {
            if (type == ConditionSourceType::NumSourceTypes) return false;
            if (type == ConditionSourceType::Seconds) return false;
            compBool = true;
            inverse = _inverse;
            isConfigured = true;
            return true;
        }

        virtual bool setCompNumber(double _number, bool _lessThan)
        {
            if (type == ConditionSourceType::NumSourceTypes) return false;
            number = _number;
            lessThan = _lessThan;
            isConfigured = true;
            return true;
        }

        virtual bool isActive(const zcm::LogEvent* le) const
        {
            assert(isFullySpecified());

            uint64_t leTimestamp = (uint64_t)le->timestamp;

            // Note this would need to move to a class variable if we wanted
            // to be able to process more than one log in one call of this
            // program. No other tools in zcm have that functionality so
            // ignoring that for now.
            static uint64_t firstUtime = std::numeric_limits<uint64_t>::max();
            if (leTimestamp < firstUtime) firstUtime = leTimestamp;

            switch (type) {
                case ConditionSourceType::Seconds: {
                    return numberIsActive((double)(leTimestamp - firstUtime) / 1e6);
                }
                case ConditionSourceType::Field: {
                    if (compBool) {
                        auto v = extractBool(le);
                        if (!v.isValid) return false;
                        return boolIsActive(v.val);
                    } else {
                        auto v = extractNumber(le);
                        if (!v.isValid) return false;
                        return numberIsActive(v.val);
                    }
                }
                case ConditionSourceType::NumSourceTypes: {
                    assert(false && "Should not be possible");
                    break;
                }
            }

            assert(false && "Unreachable");
            return false;
        }

        virtual void dump(size_t hangingIndent) const
        {
            string idt = string(hangingIndent, ' ');

            cout << idt << "Is configured: " << (isConfigured ? "true" : "false") << endl;

            cout << idt << "Type: ";
            switch (type) {
                case ConditionSourceType::Seconds:
                    cout << "Seconds" << endl;
                    break;
                case ConditionSourceType::Field:
                    cout << "Field" << endl;
                    cout << idt << "Channel: " << channel << endl;
                    cout << idt << "Msgtype: " << msgtype << endl;
                    cout << idt << "Msgfield: " << msgfield << endl;
                    break;
                case ConditionSourceType::NumSourceTypes: cout << "Unconfigured"; break;
            }

            cout << idt << "Comparator: " << (compBool ? "Boolean" : "Numeric") << endl;

            if (compBool) {
                cout << idt << "Inverse: " << (inverse ? "true" : "false") << endl;
            } else {
                cout << idt << "Number: " << number << endl;
                cout << idt << "Less than: " << (lessThan ? "true" : "false") << endl;
            }

        }
    };

    class CompoundCondition : public Condition
    {
        bool _and; // false implies this is an "or" condition

      public:
        unique_ptr<Condition> cond1;
        unique_ptr<Condition> cond2;

        CompoundCondition(bool _and) : _and(_and) {}
        virtual ~CompoundCondition() {}

        bool isFullySpecified() const override
        {
            return cond1 && cond1->isFullySpecified() &&
                   cond2 && cond2->isFullySpecified();
        }

        bool setSeconds() override
        {
            if (!cond1) cond1.reset(new Condition());
            if (!cond1->isFullySpecified()) return cond1->setSeconds();
            if (!cond2) cond2.reset(new Condition());
            return cond2->setSeconds();
        }

        bool setField(const string& field, const TypeDb* types) override
        {
            if (!cond1) cond1.reset(new Condition());
            if (!cond1->isFullySpecified()) return cond1->setField(field, types);
            if (!cond2) cond2.reset(new Condition());
            return cond2->setField(field, types);
        }

        bool setCompound(bool _and) override
        {
            if (!cond1) cond1.reset(new CompoundCondition(_and));
            if (!cond1->isFullySpecified()) return false;
            if (!cond2) cond2.reset(new CompoundCondition(_and));
            return false;
        }

        bool setCompBool(bool inverse) override
        {
            if (!cond1) cond1.reset(new Condition());
            if (!cond1->isFullySpecified()) return cond1->setCompBool(inverse);
            if (!cond2) cond2.reset(new Condition());
            return cond2->setCompBool(inverse);
        }

        bool setCompNumber(double number, bool lessThan) override
        {
            if (!cond1) cond1.reset(new Condition());
            if (!cond1->isFullySpecified()) return cond1->setCompNumber(number, lessThan);
            if (!cond2) cond2.reset(new Condition());
            return cond2->setCompNumber(number, lessThan);
        }

        bool isActive(const zcm::LogEvent* le) const override
        {
            bool c1 = cond1->isActive(le);
            bool c2 = cond2->isActive(le);
            return _and ? c1 && c2 : c1 || c2;
        }

        virtual void dump(size_t hangingIndent) const
        {
            string idt = string(hangingIndent, ' ');
            string nidt = string(hangingIndent + 2, ' ');
            size_t nextHangingIndent = hangingIndent + 4;

            cout << idt << (_and ? "And:" : "Or:") << endl;
            cout << nidt << "Cond1: ";
            if (cond1) {
                cout << endl;
                cond1->dump(nextHangingIndent);
            } else {
                cout << "\033[31m" << "unspecified" << "\033[0m" << endl;
            }
            cout << nidt << "Cond2: ";
            if (cond2) {
                cout << endl;
                cond2->dump(nextHangingIndent);
            } else {
                cout << "\033[31m" << "unspecified" << "\033[0m" << endl;
            }
        }
    };

    class Region
    {
      public:
        unique_ptr<Condition> begin;
        unique_ptr<Condition> end;

        mutable bool active = false;

        unordered_set<string> channels;

        bool isActive(const zcm::LogEvent* le) const
        {
            bool b = !begin || begin->isActive(le);
            bool e = !end || end->isActive(le);

            if (!begin) return e;
            if (b) active = true;
            if (end && e) active = false;
            return active;
        }

        bool isFullySpecified() const
        {
            return (!begin || begin->isFullySpecified()) &&
                   (!end || end->isFullySpecified());
        }

        void dump(size_t hangingIndent) const
        {
            string idt = string(hangingIndent, ' ');
            size_t nextHangingIndent = hangingIndent + 2;
            string nidt = string(nextHangingIndent, ' ');

            cout << idt << "Channels: " << endl;
            for (auto& c : channels) cout << nidt << c << endl;
            cout << idt << "Begin: ";
            if (begin) {
                cout << endl;
                begin->dump(nextHangingIndent);
            } else {
                cout << "beginning of log" << endl;
            }
            cout << idt << "End: ";
            if (end) {
                cout << endl;
                end->dump(nextHangingIndent);
            } else {
                cout << "end of log" << endl;
            }
        }
    };

    class RegionsFactory
    {
        bool specifyingEnd = true;

      public:
        vector<Region> regions;

        RegionsFactory() {}

        bool addBegin()
        {
            regions.emplace_back();
            specifyingEnd = false;
            return true;
        }

        bool addEnd()
        {
            if (regions.empty()) return false;
            auto& back = regions.back();
            if (back.end) return false;

            specifyingEnd = true;
            return true;
        }

        bool addCompound(bool _and)
        {
            if (regions.empty()) return false;

            auto& back = regions.back();

            if (!specifyingEnd) {
                if (!back.begin) {
                    back.begin.reset(new CompoundCondition(_and));
                } else {
                    back.begin->setCompound(_and);
                }
            } else {
                if (!back.end) {
                    back.end.reset(new CompoundCondition(_and));
                } else {
                    back.end->setCompound(_and);
                }
            }

            return true;
        }

        bool setConditionAsTime()
        {
            if (regions.empty()) return false;

            auto& back = regions.back();

            if (!specifyingEnd) {
                if (!back.begin) back.begin.reset(new Condition());
                if (!back.begin->setSeconds()) return false;
            } else {
                if (!back.end) back.end.reset(new Condition());
                if (!back.end->setSeconds()) return false;
            }

            return true;
        }

        bool setConditionAsField(const string& field, const TypeDb* types)
        {
            if (regions.empty()) return false;

            auto& back = regions.back();

            if (!specifyingEnd) {
                if (!back.begin) back.begin.reset(new Condition());
                if (!back.begin->setField(field, types)) return false;
            } else {
                if (!back.end) back.end.reset(new Condition());
                if (!back.end->setField(field, types)) return false;
            }

            return true;
        }

        bool setConditionCompBool(bool inverse)
        {
            if (regions.empty()) return false;

            auto& back = regions.back();

            if (!specifyingEnd) {
                if (!back.begin) back.begin.reset(new Condition());
                if (!back.begin->setCompBool(inverse)) return false;
            } else {
                if (!back.end) back.end.reset(new Condition());
                if (!back.end->setCompBool(inverse)) return false;
            }

            return true;
        }

        bool setConditionCompNumber(double number, bool lessThan)
        {
            if (regions.empty()) return false;

            auto& back = regions.back();

            if (!specifyingEnd) {
                if (!back.begin) back.begin.reset(new Condition());
                if (!back.begin->setCompNumber(number, lessThan)) return false;
            } else {
                if (!back.end) back.end.reset(new Condition());
                if (!back.end->setCompNumber(number, lessThan)) return false;
            }

            return true;
        }

        bool setChannels(const string& _channels)
        {
            if (regions.empty()) return false;
            auto& back = regions.back();
            auto chans = StringUtil::split(_channels, ',');
            for (const auto& c : chans) back.channels.insert(c);
            return true;
        }

        void dump()
        {
            cout << "Regions: " << endl;
            for (size_t i = 0; i < regions.size(); ++i) {
                cout << "  Region " << i << endl;
                regions[i].dump(4);
            }
        }
    };
    RegionsFactory factory;

    bool parse(int argc, char *argv[])
    {
        // set some defaults
        const char *optstring = "i:o:t:dbec:arsf:n:l:g:h";
        struct option long_opts[] = {
            { "input",     required_argument, 0, 'i' },
            { "output",    required_argument, 0, 'o' },
            { "type-path", required_argument, 0, 't' },
            { "debug",     no_argument,       0, 'd' },
            { "help",      no_argument,       0, 'h' },

            { "begin", no_argument, 0, 'b' },
            { "end",   no_argument, 0, 'e' },
            { "channels", required_argument, 0, 'c' },

            { "and", no_argument, 0, 'a' },
            { "or",  no_argument, 0, 'r' },

            { "seconds",       no_argument, 0, 's' },
            { "field",   required_argument, 0, 'f' },

            { "is-true",      required_argument, 0, 'n' },
            { "less-than",    required_argument, 0, 'l' },
            { "greater-than", required_argument, 0, 'g' },

            { 0, 0, 0, 0 }
        };

        int c;
        while ((c = getopt_long (argc, argv, optstring, long_opts, 0)) >= 0) {
            switch (c) {
                case 'i': inlog  = string(optarg); break;
                case 'o': outlog = string(optarg); break;
                case 't': {
                    types.reset(new TypeDb(string(optarg), debug));
                    break;
                }
                case 'd': debug = true; break;

                case 'b': {
                    if (!factory.addBegin()) {
                        cerr << "Improperly placed begin condition" << endl;
                        return false;
                    }
                    break;
                }
                case 'e': {
                    if (!factory.addEnd()) {
                        cerr << "Improperly placed end condition" << endl;
                        return false;
                    }
                    break;
                }
                case 'c': {
                    if (!factory.setChannels(optarg)) {
                        cerr << "Improperly placed channels" << endl;
                        return false;
                    }
                    break;
                }

                case 'a': {
                    if (!factory.addCompound(true)) {
                        cerr << "Improperly placed and condition" << endl;
                        return false;
                    }
                    break;
                }
                case 'r': {
                    if (!factory.addCompound(false)) {
                        cerr << "Improperly placed or condition" << endl;
                        return false;
                    }
                    break;
                }

                case 's': {
                    if (!factory.setConditionAsTime()) {
                        cerr << "Improperly specified time condition" << endl;
                        return false;
                    }
                    break;
                }
                case 'f': {
                    if (!types) {
                        cerr << "Must specify types.so before any field conditions" << endl;
                        return false;
                    }
                    if (!factory.setConditionAsField(optarg, types.get())) {
                        cerr << "Improperly specified field condition" << endl;
                        return false;
                    }
                    break;
                }
                case 'n': {
                    bool invert;
                    if (string(optarg) == "inverted") {
                        invert = true;
                    } else if (string(optarg) == "normal") {
                        invert = false;
                    } else {
                        cerr << "Invalid boolean specifier: " << optarg << endl;
                        return false;
                    }
                    if (!factory.setConditionCompBool(invert)) {
                        cerr << "Improperly specified bool comparison" << endl;
                        return false;
                    }
                    break;
                }
                case 'l': {
                    if (!factory.setConditionCompNumber(atof(optarg), true)) {
                        cerr << "Improperly specified less than comparison" << endl;
                        return false;
                    }
                    break;
                }
                case 'g': {
                    if (!factory.setConditionCompNumber(atof(optarg), false)) {
                        cerr << "Improperly specified greater equal comparison" << endl;
                        return false;
                    }
                    break;
                }

                case 'h': default: usage(); return false;
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

        if (factory.regions.empty()) {
            cerr << "Please specify at least one region" << endl;
            return false;
        }

        for (size_t i = 0; i < factory.regions.size(); ++i) {
            if (!factory.regions[i].isFullySpecified()) {
                cerr << "Region " << i << " is not fully specified" << endl;
                return false;
            }
        }

        return true;
    }

    void usage()
    {
        cout << "usage: zcm-log-filter [options]" << endl
             << "" << endl
             << "    Filter a log to events in regions marked by begin and end conditions" << endl
             << endl
             << "Example:" << endl
             << "    # Filters a log to all events between 10 and 20 seconds after log start" << endl
             << "    zcm-log-filter -l zcm.log -o out.log -b -t 10 -e -t 20" << endl
             << endl
             << "Options:" << endl
             << endl
             << "  -h, --help              Shows this help text and exits" << endl
             << "  -i, --input=logfile     Input log to filter" << endl
             << "  -o, --output=logfile    Output filtered log file" << endl
             << "  -t, --type-path=path    Path to shared library containing zcmtypes" << endl
             << "  -d, --debug             Run a dry run to ensure proper setup" << endl
             << endl
             << "  Conditions" << endl
             << "    -b             Interpret the next args as defining begin conditions" << endl
             << "    -e             Interpret the next args as defining end conditions" << endl
             << "    -c             Channels to keep when inside region" << endl
             << endl
             << "    -s             Condition is based on seconds since beginning of zcm log." << endl
             << "    -f <flag>      A config-style description of a flag in a channel/message" << endl
             << "                   Formatted as: CHANNEL_NAME:MESSAGE_TYPE_LONG_NAME:FIELD" << endl
             << "                   Where FIELD can be nested fields using \".\" as the" << endl
             << "                   nested field accessor." << endl
             << "                   Condition will be evaluated as boolean unless one of" << endl
             << "                   the following args is specified" << endl
             << endl
             << "    -n <inverted>  Evaluate condition as \"FIELD == true\"  for -b normal" << endl
             << "                                      as \"FIELD == false\" for -b inverted" << endl
             << "                   All other strings after -n are invalid" << endl
             << "    -l <number>    Evaluate condition as \"FIELD < number\"" << endl
             << "                   Currently only supports doubles" << endl
             << "    -g <number>    Evaluate condition as \"FIELD >= number\"" << endl
             << "                   Currently only supports doubles" << endl
             << endl
             << "    Compounds:" << endl
             << "      -a  And" << endl
             << "      -r  Or" << endl
             << endl
             << "       For compound expressions, put the compound specifier before the two" << endl
             << "       expressions you want it to act on." << endl
             << endl
             << "       For example:" << endl
             << endl
             << "       zcm-log-filter -i in.log -o out.log -b -a -s 10 -s 20 -e -s 100" << endl
             << endl << endl;
    }
};

int main(int argc, char* argv[])
{
    Args args;
    bool success = args.parse(argc, argv);
    cout << endl;
    args.factory.dump();
    cout << endl;
    if (!success) return 1;

    zcm::LogFile inlog(args.inlog, "r");
    if (!inlog.good()) {
        cerr << "Unable to open input zcm log: " << args.inlog << endl;
        return 1;
    }
    fseeko(inlog.getFilePtr(), 0, SEEK_END);
    off64_t logSize = ftello(inlog.getFilePtr());
    fseeko(inlog.getFilePtr(), 0, SEEK_SET);

    zcm::LogFile outlog(args.outlog, "w");
    if (!outlog.good()) {
        cerr << "Unable to open output zcm log: " << args.outlog << endl;
        return 1;
    }

    if (args.debug) return 0;

    size_t numInEvents = 0, numOutEvents = 0;
    const zcm::LogEvent* evt;
    off64_t offset;

    static int lastPrintPercent = 0;
    while (1) {
        offset = ftello(inlog.getFilePtr());

        int percent = 100.0 * offset / (logSize == 0 ? 1 : logSize);
        if (percent != lastPrintPercent) {
            cout << "\r" << "Percent Complete: " << percent << flush;
            lastPrintPercent = percent;
        }

        evt = inlog.readNextEvent();
        if (evt == nullptr) break;

        bool keepEvent = false;
        for (const auto& r : args.factory.regions) {
            if (r.isActive(evt)) {
                keepEvent = r.channels.empty() ||
                            r.channels.find(evt->channel) != r.channels.end();
                // Intentionally not breaking here so every condition can see every event
            }
        }

        if (keepEvent) {
            outlog.writeEvent(evt);
            numOutEvents++;
        }

        numInEvents++;
    }
    if (lastPrintPercent != 100) cout << "\r" << "Percent Complete: 100" << flush;
    cout << endl;

    inlog.close();
    outlog.close();

    cout << "Filtered " << numInEvents << " events down to "
         << numOutEvents << " events" << endl;

    return 0;
}
