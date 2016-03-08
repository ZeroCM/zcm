#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cassert>
#include <cinttypes>
#include <regex>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <queue>
#include <signal.h>

#include <errno.h>
#include <time.h>
#include <getopt.h>

#include "zcm/zcm.h"
#include "zcm/eventlog.h"
#include "zcm/util/debug.h"

#include <string>
#include "util/FileUtil.hpp"
#include "util/TimeUtil.hpp"
#include "util/Types.hpp"

using namespace std;

#include "platform.hpp"

static atomic_int done {0};

struct Args
{
    double auto_split_mb      = 0.0;
    bool   force_overwrite    = false;
    string chan               = ".*";
    bool   auto_increment     = false;
    bool   use_strftime       = false;
    string zcmurl             = "";
    bool   quiet              = false;
    bool   invert_channels    = false;
    int    rotate             = -1;
    int    fflush_interval_ms = 100;
    i64    max_target_memory  = 0;

    string input_fname;

    bool parse(int argc, char *argv[])
    {
        // set some defaults
        const char *optstring = "hb:c:fi:u:r:s:qvl:m:";
        struct option long_opts[] = {
            { "help", no_argument, 0, 'h' },
            { "split-mb", required_argument, 0, 'b' },
            { "channel", required_argument, 0, 'c' },
            { "force", no_argument, 0, 'f' },
            { "increment", required_argument, 0, 'i' },
            { "zcm-url", required_argument, 0, 'u' },
            { "rotate", required_argument, 0, 'r' },
            { "strftime", required_argument, 0, 's' },
            { "quiet", no_argument, 0, 'q' },
            { "invert-channels", no_argument, 0, 'v' },
            { "flush-interval", required_argument, 0, 'l'},
            { "max-target-memory", required_argument, 0, 'm'},
            { 0, 0, 0, 0 }
        };

        int c;
        while ((c = getopt_long (argc, argv, optstring, long_opts, 0)) >= 0) {
            switch (c) {
                case 'b':
                    auto_split_mb = strtod(optarg, NULL);
                    if (auto_split_mb <= 0)
                        return false;
                    break;
                case 'f':
                    force_overwrite = 1;
                    break;
                case 'c':
                    chan = optarg;
                    break;
                case 'i':
                    auto_increment = true;
                    break;
                case 's':
                    use_strftime = true;
                    break;
                case 'u':
                    zcmurl = optarg;
                    break;
                case 'q':
                    quiet = true;
                    break;
                case 'v':
                    invert_channels = true;
                    break;
                case 'r': {
                    char* eptr = NULL;
                    rotate = strtol(optarg, &eptr, 10);
                    if(*eptr)
                        return false;
                } break;
                case 'l':
                    fflush_interval_ms = atol(optarg);
                    if (fflush_interval_ms <= 0)
                        return false;
                    break;
                case 'm':
                    max_target_memory = atoll(optarg);
                    break;
                case 'h':
                default:
                    return false;
            };
        }

        if (optind == argc) {
            input_fname = "zcmlog-%Y-%m-%d";
            auto_increment = true;
            use_strftime = true;
        } else if (optind == argc - 1) {
            input_fname = argv[optind];
        } else if (optind < argc-1) {
            return false;
        }

        if (auto_split_mb > 0 && !(auto_increment || (rotate > 0))) {
            fprintf(stderr, "ERROR.  --split-mb requires either --increment or --rotate\n");
            return false;
        }

        if (rotate > 0 && auto_increment) {
            fprintf(stderr, "ERROR.  --increment and --rotate can't both be used\n");
            return false;
        }

        return true;
    }
};

struct Logger
{
    Args   args;

    string filename;
    string fname_prefix;

    zcm_eventlog_t *log             = nullptr;

    int next_increment_num          = 0;

    // variables for inverted matching (e.g., logging all but some channels)
    regex invert_regex;

    // these members controlled by writing
    size_t nevents                  = 0;
    size_t logsize                  = 0;
    size_t events_since_last_report = 0;
    u64    last_report_time         = 0;
    size_t last_report_logsize      = 0;
    u64    time0                    = TimeUtil::utime();
    u64    last_fflush_time         = 0;

    size_t dropped_packets_count    = 0;
    u64    last_drop_report_utime   = 0;
    size_t last_drop_report_count   = 0;

    int    num_splits               = 0;

    i64    totalMemoryUsage         = 0;

    mutex lk;
    condition_variable newEventCond;

    queue<zcm_eventlog_event_t*> q;

    Logger() {}

    ~Logger()
    {
        while (!q.empty()) {
            zcm_eventlog_event_t* le = q.front();
            q.pop();
            delete[] le->channel;
            delete[] (char*)le->data;
            delete le;
        }
    }

    bool init(int argc, char *argv[])
    {
        if (!args.parse(argc, argv))
            return false;

        if (!openLogfile())
            return false;

        // Compile the regex if we are in invert mode
        if (args.invert_channels) {
            invert_regex = regex{args.chan};
        }

        return true;
    }

    const char *getSubChannel()
    {
        // if inverting the channels, subscribe to everything and invert on the callback
        return (!args.invert_channels) ? args.chan.c_str() : ".*";
    }

    void rotate_logfiles()
    {
        if (!args.quiet)
            printf("Rotating log files\n");

        // delete log files that have fallen off the end of the rotation
        string tomove = fname_prefix + "." + to_string(args.rotate-1);
        if (FileUtil::exists(tomove)) {
            if (0 != FileUtil::remove(tomove)) {
                fprintf(stderr, "ERROR! Unable to delete [%s]\n", tomove.c_str());
            }
        }

        // Rotate away any existing log files
        for (int file_num = args.rotate-1; file_num >= 0; file_num--) {
            string newname = fname_prefix + "." + to_string(file_num);
            string tomove  = fname_prefix + "." + to_string(file_num-1);
            if (FileUtil::exists(tomove)) {
                if (0 != FileUtil::rename(tomove, newname)) {
                    fprintf(stderr, "ERROR!  Unable to rotate [%s]\n", tomove.c_str());
                }
            }
        }
    }

    bool openLogfile()
    {
        char tmp_path[PATH_MAX];

        // maybe run the filename through strftime
        if (args.use_strftime) {
            time_t now = time (NULL);
            strftime(tmp_path, sizeof(tmp_path),
                     args.input_fname.c_str(), localtime(&now));
            string new_prefix = tmp_path;

            // If auto-increment is enabled and the strftime-formatted filename
            // prefix has changed, then reset the auto-increment counter.
            if (args.auto_increment && fname_prefix != new_prefix)
                next_increment_num = 0;
            fname_prefix = std::move(new_prefix);
        } else {
            fname_prefix = args.input_fname;
        }

        if (args.auto_increment) {
            /* Loop through possible file names until we find one that doesn't
             * already exist.  This way, we never overwrite an existing file. */
            do {
                snprintf(tmp_path, sizeof(tmp_path), "%s.%02d",
                         fname_prefix.c_str(), next_increment_num);
                filename = tmp_path;
                next_increment_num++;
            } while(FileUtil::exists(filename));
        } else if (args.rotate > 0) {
            filename = fname_prefix + ".0";
        } else {
            filename = fname_prefix;
            if (!args.force_overwrite) {
                if (FileUtil::exists(filename)) {
                    fprintf(stderr, "Refusing to overwrite existing file \"%s\"\n", filename.c_str());
                    return false;
                }
            }
        }

        // create directories if needed
        string dirpart = FileUtil::dirname(filename);
        if (!FileUtil::dirExists(dirpart))
            FileUtil::mkdirWithParents(dirpart, 0755);

        if(!args.quiet) {
            printf("Opening log file \"%s\"\n", filename.c_str());
        }

        // open output file in append mode if we're rotating log files, or write
        // mode if not.
        const char *logmode = (args.rotate > 0) ? "a" : "w";
        log = zcm_eventlog_create(filename.c_str(), logmode);
        if (!log) {
            perror("Error: fopen failed");
            return false;
        }
        return true;
    }

    static void handler(const zcm_recv_buf_t *rbuf, const char *channel, void *usr)
    { ((Logger*)usr)->handler_(rbuf, channel); }

    void handler_(const zcm_recv_buf_t *rbuf, const char *channel)
    {
        if (args.invert_channels) {
            cmatch match;
            regex_match(channel, match, invert_regex);
            if (match.size() > 0)
                return;
        }

        bool stillRoom;
        {
            unique_lock<mutex> lock{lk};
            stillRoom = (args.max_target_memory == 0) ? true :
                (totalMemoryUsage + rbuf->data_size < args.max_target_memory);
        }

        zcm_eventlog_event_t *le;
        if (stillRoom) {
            le = new zcm_eventlog_event_t();
            le->timestamp = rbuf->recv_utime;
            le->channellen = strlen(channel);
            le->datalen = rbuf->data_size;
            le->channel = new char[le->channellen + 1];
            le->channel[le->channellen] = '\0'; // terminate the cstr with null char
            memcpy(le->channel, channel, sizeof(char) * le->channellen);
            le->data = new char[rbuf->data_size];
            memcpy(le->data, rbuf->data, sizeof(char) * rbuf->data_size);
        }

        {
            unique_lock<mutex> lock{lk};
            if (stillRoom) {
                q.push(le);
                totalMemoryUsage += le->datalen + le->channellen + sizeof(zcm_eventlog_event_t);
            } else {
                ZCM_DEBUG("Dropping message due to enforced memory constraints");
                ZCM_DEBUG("Current memory estimations are at %" PRId64 " bytes", totalMemoryUsage);
            }
        }
        newEventCond.notify_all();
    }

    void flushWhenReady()
    {
        zcm_eventlog_event_t *le = nullptr;
        size_t qSize = 0;
        i64 memUsed = 0;
        {
            unique_lock<mutex> lock{lk};

            while (q.empty()) {
                if (done) return;
                newEventCond.wait(lock);
            }
            if (done) return;

            le = q.front();
            q.pop();
            qSize = q.size();
            memUsed = totalMemoryUsage; // want to capture the max mem used, not post flush
            totalMemoryUsage -= (le->datalen + le->channellen + sizeof(zcm_eventlog_event_t));
        }
        if (qSize != 0) ZCM_DEBUG("Queue size = %zu\n", qSize);

        // Is it time to start a new logfile?
        if (args.auto_split_mb) {
            double logsize_mb = (double)logsize / (1 << 20);
            if (logsize_mb > args.auto_split_mb) {
                // Yes.  open up a new log file
                zcm_eventlog_destroy(log);
                if (args.rotate > 0)
                    rotate_logfiles();
                if(!openLogfile())
                    exit(1);
                num_splits++;
                logsize = 0;
                last_report_logsize = 0;
            }
        }

        if (zcm_eventlog_write_event(log, le) != 0) {
            static u64 last_spew_utime = 0;
            string reason = strerror(errno);
            u64 now = TimeUtil::utime();
            if (now - last_spew_utime > 500000) {
                fprintf(stderr, "zcm_eventlog_write_event: %s\n", reason.c_str());
                last_spew_utime = now;
            }
            if (errno == ENOSPC)
                exit(1);

            delete[] le->channel;
            delete[] (char*)le->data;
            delete le;
            return;
        }

        if (args.fflush_interval_ms >= 0 &&
            (le->timestamp - last_fflush_time) > (u64)args.fflush_interval_ms * 1000) {
            Platform::fflush(zcm_eventlog_get_fileptr(log));
            last_fflush_time = le->timestamp;
        }

        // bookkeeping, cleanup
        nevents++;
        events_since_last_report++;
        logsize += 4 + 8 + 8 + 4 + le->channellen + 4 + le->datalen;

        i64 offset_utime = le->timestamp - time0;
        if (!args.quiet && (offset_utime - last_report_time > 1000000)) {
            double dt = (offset_utime - last_report_time)/1000000.0;

            double tps =  events_since_last_report / dt;
            double kbps = (logsize - last_report_logsize) / dt / 1024.0;
            printf("Summary: %s ti:%4" PRId64 " sec  |  Events: %-9zu ( %4zu MB )  |  "
                   "TPS: %8.2f  |  KB/s: %8.2f  |  Buf Size: % 8" PRId64 " KB\n",
                   filename.c_str(),
                   offset_utime / 1000000,
                   nevents, logsize/1048576,
                   tps, kbps, memUsed / 1024);
            last_report_time = offset_utime;
            events_since_last_report = 0;
            last_report_logsize = logsize;
        }

        delete[] le->channel;
        delete[] (char*)le->data;
        delete le;
    }

    void wakeup()
    {
        unique_lock<mutex> lock(lk);
        newEventCond.notify_all();
    }
};

Logger logger{};

void sighandler(int signal)
{
    done++;
    logger.wakeup();
    if (done == 3) exit(1);
}

static void usage()
{
    fprintf(stderr, "usage: zcm-logger [options] [FILE]\n"
            "\n"
            "    ZCM message logging utility.  Subscribes to all channels on an ZCM\n"
            "    network, and records all messages received on that network to\n"
            "    FILE.  If FILE is not specified, then a filename is automatically\n"
            "    chosen.\n"
            "\n"
            "Options:\n"
            "\n"
            "  -c, --channel=CHAN         Channel string to pass to zcm_subscribe.\n"
            "                             (default: \".*\")\n"
            "  -l, --flush-interval=MS    Flush the log file to disk every MS milliseconds.\n"
            "                             (default: 100)\n"
            "  -f, --force                Overwrite existing files\n"
            "  -h, --help                 Shows this help text and exits\n"
            "  -i, --increment            Automatically append a suffix to FILE\n"
            "                             such that the resulting filename does not\n"
            "                             already exist.  This option precludes -f and\n"
            "                             --rotate\n"
            "  -u, --zcm-url=URL          Log messages on the specified ZCM URL\n"
            "  -m, --max-unwritten-mb=SZ  Maximum size of received but unwritten\n"
            "                             messages to store in memory before dropping\n"
            "                             messages.  (default: 100 MB)\n"
            "  -r, --rotate=NUM           When creating a new log file, rename existing files\n"
            "                             out of the way and always write to FILE.0.  If\n"
            "                             FILE.0 already exists, it is renamed to FILE.1.  If\n"
            "                             FILE.1 exists, it is renamed to FILE.2, etc.  If\n"
            "                             FILE.NUM exists, then it is deleted.  This option\n"
            "                             precludes -i.\n"
            "  -b, --split-mb=N           Automatically start writing to a new log\n"
            "                             file once the log file exceeds N MB in size\n"
            "                             (can be fractional).  This option requires -i\n"
            "                             or --rotate.\n"
            "  -q, --quiet                Suppress normal output and only report errors.\n"
            "  -s, --strftime             Format FILE with strftime.\n"
            "  -v, --invert-channels      Invert channels.  Log everything that CHAN\n"
            "                             does not match.\n"
            "  -m, --max-target-memory    Attempt to limit the total buffer usage to this\n"
            "                             amount of memory. If specified, ensure that this\n"
            "                             number is at least as large as the maximum message\n"
            "                             size you expect to receive. This argument is\n"
            "                             specified in bytes. Suffixes are not yet supported.\n"
            "\n"
            "Rotating / splitting log files\n"
            "==============================\n"
            "    For long-term logging, zcm-logger can rotate through a fixed number of\n"
            "    log files, moving to a new log file as existing files reach a maximum size.\n"
            "    To do this, use --rotate and --split-mb.  For example:\n"
            "\n"
            "        # Rotate through logfile.0, logfile.1, ... logfile.4\n"
            "        zcm-logger --rotate=5 --split-mb=2 logfile\n"
            "\n"
            "    Moving to a new file happens either when the current log file size exceeds\n"
            "    the limit specified by --split-mb, or when zcm-logger receives a SIGHUP.\n"
            "\n");
}

int main(int argc, char *argv[])
{
    Platform::setstreambuf();

    if (!logger.init(argc, argv)) {
        usage();
        return 1;
    }

    // begin logging
    zcm_t *zcm = zcm_create(logger.args.zcmurl == "" ? nullptr : logger.args.zcmurl.c_str());
    if (!zcm) {
        fprintf(stderr, "Couldn't initialize ZCM!\n");
        if (logger.args.zcmurl != "") {
            fprintf(stderr, "Unable to parse url: %s\n", logger.args.zcmurl.c_str());
            fprintf(stderr, "Try running with ZCM_DEBUG=1 for more info\n");
        } else {
            fprintf(stderr, "Please provide a valid zcm url either with the ZCM_DEFAULT_URL\n"
                            "environment variable, or with the '-u' command line argument.\n");
        }
        return 1;
    }

    zcm_subscribe(zcm, logger.getSubChannel(), Logger::handler, &logger);

    // Register signal handlers
    signal(SIGINT, sighandler);
    signal(SIGQUIT, sighandler);
    signal(SIGTERM, sighandler);

    zcm_start(zcm);

    while (!done)
        logger.flushWhenReady();

    zcm_stop(zcm);
    zcm_flush(zcm);
    zcm_destroy(zcm);

    fprintf(stderr, "Logger exiting\n");

    return 0;
}
