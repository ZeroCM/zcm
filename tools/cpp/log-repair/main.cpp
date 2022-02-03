#include <iostream>

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
    bool   overwrite = false;

    bool init(int argc, char *argv[])
    {
        struct option long_opts[] = {
            { "help",                no_argument, 0, 'h' },
            { "output",        required_argument, 0, 'o' },
            { "overwrite",           no_argument, 0, 'f' },
            { 0, 0, 0, 0 }
        };

        int c;
        while ((c = getopt_long(argc, argv, "ho:f", long_opts, 0)) >= 0) {
            switch (c) {
                case 'o':          outfile   = string(optarg); break;
                case 'f':          overwrite = true;           break;
                case 'h': default: usage(); return false;
            };
        }

        if (optind != argc - 1) {
            cerr << "Please specify a logfile" << endl;
            usage();
            return false;
        }

        infile = string(argv[optind]);

        if (overwrite) outfile = infile;

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
             << "  -f, --overwrite        write output over the input file" << endl
             << "  -h, --help             Shows some help text and exits." << endl
             << endl;
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

    cout << "zcm-log-repair done" << endl;

    return ret;
}
