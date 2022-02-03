#include <algorithm>
#include <atomic>
#include <iostream>
#include <limits>
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
    string infile    = "";
    string outfile   = "";

    bool init(int argc, char *argv[])
    {
        struct option long_opts[] = {
            { "help",                no_argument, 0, 'h' },
            { "output",        required_argument, 0, 'o' },
            { 0, 0, 0, 0 }
        };

        int c;
        while ((c = getopt_long(argc, argv, "ho:f", long_opts, 0)) >= 0) {
            switch (c) {
                case 'o':          outfile   = string(optarg); break;
                case 'h': default: usage(); return false;
            };
        }

        if (optind != argc - 1) {
            cerr << "Please specify a logfile" << endl;
            usage();
            return false;
        }

        infile = string(argv[optind]);

        if (outfile.empty()) {
            cerr << "Must specify output file" << endl;
            return false;
        }

        return true;
    }

    void usage()
    {
        cerr << "usage: zcm-log-repair [options] [FILE]" << endl
             << "" << endl
             << "    Reads packets from an ZCM log file and re-writes that log file" << endl
             << "    ensuring that all events are stored in recv_utime order." << endl
             << "    This is generally only required if the original log was captured" << endl
             << "    using multiple zcm shards and the user requires a strict ordering" << endl
             << "    of events in the log." << endl
             << "" << endl
             << "Options:" << endl
             << "" << endl
             << "  -o, --output=filename  specify output file" << endl
             << "  -h, --help             Shows some help text and exits." << endl
             << endl;
    }
};

struct LogRepair
{
    Args args;
    zcm::LogFile* logIn  = nullptr;
    zcm::LogFile* logOut = nullptr;
    zcm::LogFile* logVer = nullptr;

    const zcm::LogEvent*         event;
    off_t                        length;
    off_t                        offset;
    vector<pair<int64_t, off_t>> timestamps;
    size_t                       progress;

    LogRepair() { }

    ~LogRepair()
    {
        if (logIn)  {
            if (logIn->good()) logIn->close();
            delete  logIn;
        }
        if (logOut) {
            if (logOut->good()) logOut->close();
            delete logOut;
        }
        if (logVer) {
            if (logVer->good()) logVer->close();
            delete logVer;
        }
    }

    bool init(int argc, char *argv[])
    {
        if (!args.init(argc, argv))
            return false;

        logIn = new zcm::LogFile(args.infile, "r");
        if (!logIn->good()) {
            cerr << "Error: Failed to open '" << args.infile << "'" << endl;
            return false;
        }

        logOut = new zcm::LogFile(args.outfile, "w");
        if (!logOut->good()) {
            cerr << "Error: Failed to create output log" << endl;
            return false;
        }

        fseeko(logIn->getFilePtr(), 0, SEEK_END);
        length = ftello(logIn->getFilePtr());
        fseeko(logIn->getFilePtr(), 0, SEEK_SET);

        timestamps.reserve(1e6);

        return true;
    }

    int run()
    {
        cout << "Reading log" << endl;
        progress = 0;
        cout << progress << "%" << flush;
        while (true) {
            if (done) return 1;

            offset = ftello(logIn->getFilePtr());
            event  = logIn->readNextEvent();
            if (!event) break;

            timestamps.emplace_back(event->timestamp, offset);

            size_t p = (size_t)((100 * offset) / length);
            if (p != progress) {
                progress = p;
                cout << "\r" << progress << "%" << flush;
            }
        }
        cout << endl << "Read " << timestamps.size() << " events" << endl;

        cout << "Repairing log data" << endl;
        sort(timestamps.begin(), timestamps.end());

        cout << "Writing new log" << endl;
        progress = 0;
        cout << progress << "%" << flush;
        for (size_t i = 0; i < timestamps.size(); ++i) {
            if (done) return 1;

            size_t p = (100 * i) / timestamps.size();
            if (p != progress) {
                progress = p;
                cout << "\r" << progress << "%" << flush;
            }

            logOut->writeEvent(logIn->readEventAtOffset(timestamps[i].second));
        }

        cout << endl << "Flushing to disk" << endl;
        logOut->close();

        cout << "Verifying output" << endl;
        logVer = new zcm::LogFile(args.outfile, "r");
        if (!logVer->good()) {
            cerr << "Error: Failed to open output log for verification" << endl;
            return 1;
        }

        size_t i = 0;
        progress = 0;
        cout << progress << "%" << flush;
        while (true) {
            if (done) return 1;

            event = logIn->readNextEvent();
            if (event->timestamp != timestamps[i++].first) {
                cerr << endl << "Error: output log timestamp mismatch" << endl;
                return 1;
            }

            size_t p = (100 * i) / timestamps.size();
            if (p != progress) {
                progress = p;
                cout << "\r" << progress << "%" << flush;
            }
        }

        return 0;
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
