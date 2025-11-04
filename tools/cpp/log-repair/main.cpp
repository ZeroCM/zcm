#include <algorithm>
#include <atomic>
#include <iostream>
#include <limits>
#include <memory>
#include <utility>
#include <vector>

#include <getopt.h>
#include <signal.h>
#include <unistd.h>

#include <zcm/zcm-cpp.hpp>

using namespace std;

static atomic_int done {0};

static void sighandler(int signal)
{
    done++;
    if (done == 3) exit(1);
}

struct Args
{
    string  infile      = "";
    string  outfile     = "";
    int64_t splitUs     = numeric_limits<int64_t>::max();
    bool    outputSplit = false;
    bool    verify      = false;

    bool init(int argc, char *argv[])
    {
        struct option long_opts[] = {
            { "help",               no_argument, 0, 'h' },
            { "input",        required_argument, 0, 'i' },
            { "output",       required_argument, 0, 'o' },
            { "split-us",     required_argument, 0, 's' },
            { "output-split",       no_argument, 0, 'S' },
            { "verify",             no_argument, 0, 'v' },
            { 0, 0, 0, 0 }
        };

        int c;
        while ((c = getopt_long(argc, argv, "hi:o:s:Sv", long_opts, 0)) >= 0) {
            switch (c) {
                case 'i': infile      = string(optarg); break;
                case 'o': outfile     = string(optarg); break;
                case 's': splitUs     = atol(optarg); break;
                case 'S': outputSplit = true; break;
                case 'v': verify      = true; break;
                case 'h': default: usage(); return false;
            };
        }

        if (infile.empty()) {
            if (optind != argc - 1) {
                cerr << "Please specify a logfile" << endl;
                usage();
                return false;
            }
            infile = string(argv[optind]);
        }

        if (outfile.empty() && !verify) {
            cerr << "Must specify output file or verify mode" << endl;
            return false;
        }

        return true;
    }

    void usage()
    {
        cerr << "usage: zcm-log-repair [options] [FILE]" << endl
             << "" << endl
             << "    Reads packets from a ZCM log file and writes them to an output " << endl
             << "    log file ensuring that all events are stored in recv_utime order." << endl
             << "    This is generally only required if the original log was captured" << endl
             << "    using multiple zcm shards and the user requires a strict ordering" << endl
             << "    of events in the log." << endl
             << "" << endl
             << "Options:" << endl
             << "" << endl
             << "  -h, --help               Shows this help text and exits" << endl
             << "  -i, --input=filename     specify input file (alteratively can be a positional argument)" << endl
             << "  -o, --output=filename    specify output file" << endl
             << endl
             << "  -s, --split-us=delta_us  split output logs if there is a timestamp jump of >= provided magnitude" << endl
             << "                           only the largest section will be output" << endl
             << "  -S, --output-split       also output all the other files resulting from the split" << endl
             << "                           file names will be <output_filename>.[num]" << endl
             << endl
             << "  -v, --verify             verify input log is monotonic in timestamp" << endl
             << endl;
    }
};

struct LogRepair
{
    Args args;
    unique_ptr<zcm::LogFile> logIn;

    // These variables are reused between functions
    const zcm::LogEvent*         event;
    off_t                        length;
    off_t                        offset;
    size_t                       progress;

    vector<vector<pair<int64_t, off_t>>> timestamps;

    LogRepair() { }

    ~LogRepair() { }

    bool init(int argc, char *argv[])
    {
        if (!args.init(argc, argv))
            return false;

        logIn.reset(new zcm::LogFile(args.infile, "r"));
        if (!logIn->good()) {
            cerr << "Error: Failed to open '" << args.infile << "'" << endl;
            return false;
        }


        // somewhat arbitrary, but starting with a high capacity helps speed up the read-in
        timestamps.emplace_back();
        timestamps.back().reserve(1e6);

        return true;
    }

    int verifyLog(string name, const vector<pair<int64_t, off_t>>* expectedTimestamps = nullptr)
    {
        cout << "Verifying " << name << endl;
        unique_ptr<zcm::LogFile> log(new zcm::LogFile(name, "r"));
        if (!log->good()) {
            cerr << "Error: Failed to open log for verification" << endl;
            return 1;
        }

        fseeko(log->getFilePtr(), 0, SEEK_END);
        length = ftello(log->getFilePtr());
        fseeko(log->getFilePtr(), 0, SEEK_SET);

        int64_t lastTimestamp = 0;
        size_t i              = 0;
        progress              = 0;
        cout << progress << "%" << flush;
        while (true) {
            if (done) return 1;

            offset = ftello(log->getFilePtr());
            event  = log->readNextEvent();

            size_t p = (100 * offset) / length;
            if (p != progress) {
                progress = p;
                cout << "\r" << progress << "%" << flush;
            }

            if (!event) break;

            if (event->timestamp < lastTimestamp) {
                cerr << endl << "Error: " << name
                     << " event timestamp nonmonotonic at event " << i << endl;
                return 1;
            }
            lastTimestamp = event->timestamp;

            if (expectedTimestamps && i < expectedTimestamps->size() &&
                event->timestamp != (*expectedTimestamps)[i].first) {
                cerr << endl << "Error: output log timestamp mismatch" << endl;
                cerr << "Expected " << (*expectedTimestamps)[i].first << " got "
                        << event->timestamp << " (idx " << i << ")" << endl;
                return 1;
            }

            ++i;
        }
        cout << endl;

        if (expectedTimestamps && i != expectedTimestamps->size()) {
            cerr << endl << "Error: " << name << " has wrong number of events. "
                 << "Expected " << expectedTimestamps->size() << " found " << i << endl;
        }

        return 0;
    }

    int writeLog(string name, unique_ptr<zcm::LogFile>& input,
                 const vector<pair<int64_t, off_t>>& sectionTimestamps)
    {
        cout << "Writing " << sectionTimestamps.size() << " events to " << name << endl;

        unique_ptr<zcm::LogFile> logOut(new zcm::LogFile(name, "w"));
        if (!logOut->good()) {
            cerr << "Error: Failed to create output log" << endl;
            return 1;
        }

        progress = 0;
        cout << progress << "%" << flush;
        for (size_t i = 0; i < sectionTimestamps.size(); ++i) {
            if (done) return 1;

            auto* event = input->readEventAtOffset(sectionTimestamps[i].second);
            if (!event) {
                fseeko(input->getFilePtr(), 0, SEEK_END);
                length = ftello(input->getFilePtr());

                cerr << endl << "Failed to read event from input log at offset "
                     << sectionTimestamps[i].second << " / " << length << endl;

                return 1;
            }
            logOut->writeEvent(event);

            size_t p = (100 * (i + 1)) / sectionTimestamps.size();
            if (p != progress) {
                progress = p;
                cout << "\r" << progress << "%" << flush;
            }
        }

        cout << endl << "Flushing to disk" << endl;

        return 0;
    }

    int run()
    {
        if (args.verify) return verifyLog(args.infile);

        fseeko(logIn->getFilePtr(), 0, SEEK_END);
        length = ftello(logIn->getFilePtr());
        fseeko(logIn->getFilePtr(), 0, SEEK_SET);

        // XXX: Note we are reading ALL of the event timestamps into memory, so
        //      this will use something like 16 * num_events memory. In some use
        //      cases that won't be ideal, so if people are running into that, we
        //      can try a different approach.
        cout << "Reading log with length " << length << endl;
        progress = 0;
        cout << progress << "%" << flush;
        while (true) {
            if (done) return 1;

            offset           = ftello(logIn->getFilePtr());
            event            = logIn->readNextEvent();

            size_t p = (size_t)((100 * offset) / length);
            if (p != progress) {
                progress = p;
                cout << "\r" << progress << "%" << flush;
            }

            if (!event) break;

            if (timestamps.back().empty()) {
                timestamps.back().emplace_back(event->timestamp, offset);
                continue;
            }

            int64_t lastTimestamp = timestamps.back().back().first;
            int64_t delta = event->timestamp > lastTimestamp
                                ? event->timestamp - lastTimestamp
                                : lastTimestamp - event->timestamp;

            if (delta > args.splitUs) {
                timestamps.emplace_back();
                timestamps.back().reserve(1e6);
            }

            timestamps.back().emplace_back(event->timestamp, offset);
            if (timestamps.back().size() == timestamps.back().capacity()) {
                timestamps.back().reserve(timestamps.back().capacity() * 2);
            }
        }

        size_t largestIdx = 0;
        size_t largestSize = 0;
        cout << endl << "Read {";
        for (size_t i = 0; i < timestamps.size(); ++i) {
            auto& t = timestamps[i];
            if (t.size() > largestSize) {
                largestIdx  = i;
                largestSize = t.size();
            }
            cout << t.size() << ", ";
        }
        cout << "\b\b} events" << endl;

        auto process = [&](size_t idx, const string& name) {
            auto& t = timestamps[idx];

            sort(t.begin(), t.end());
            int ret = writeLog(name, logIn, t);
            if (ret != 0) return ret;

            ret = verifyLog(name, &t);
            if (ret != 0) return ret;

            return ret;
        };

        if (!args.outputSplit) return process(largestIdx, args.outfile);

        int ret = 0;
        for (size_t i = 0; i < timestamps.size(); ++i) {
            ret = process(i, args.outfile + "." + to_string(i));
            if (ret != 0) return ret;
        }
        return ret;
    }
};

int main(int argc, char* argv[])
{
    LogRepair lr;
    if (!lr.init(argc, argv)) return 1;

    // Register signal handlers
    signal(SIGINT, sighandler);
    signal(SIGQUIT, sighandler);
    signal(SIGTERM, sighandler);

    int ret = lr.run();

    if (ret == 0) {
        cout << endl << "Success" << endl;
    } else {
        cerr << endl << "Failure" << endl;
    }

    return ret;
}
