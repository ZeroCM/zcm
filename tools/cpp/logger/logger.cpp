#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cassert>
#include <cinttypes>

#include <errno.h>
#include <time.h>
#include <getopt.h>

#include "zcm/zcm.h"
#include "zcm/eventlog.h"

#include <string>
#include "util/FileUtil.hpp"
using namespace std;

#ifdef WIN32
#define __STDC_FORMAT_MACROS            // Enable integer types
// TODO re-enable windows support
//#include <zcm/windows/WinPorting.h>
#else
# include <sys/time.h>
# include <unistd.h>
# include <linux/limits.h>
#endif

//#include "glib_util.h"

typedef uint8_t  u8;
typedef  int8_t  i8;
typedef uint16_t u16;
typedef  int16_t i16;
typedef uint32_t u32;
typedef  int32_t i32;
typedef uint64_t u64;
typedef  int64_t i64;

#ifdef SIGHUP
# define USE_SIGHUP
#endif

#define DEFAULT_MAX_WRITE_QUEUE_SIZE_MB 100
#define SECONDS_PER_HOUR 3600

static int _reset_logfile = 0;

static inline u64 timestamp_now()
{
    struct timeval tv;
    gettimeofday (&tv, NULL);
    return (u64) tv.tv_sec * 1000000 + tv.tv_usec;
}

struct Logger
{
    string input_fname;
    string fname;
    string fname_prefix;

    zcm_eventlog_t *log             = nullptr;
    zcm_t    *zcm                   = nullptr;

    size_t max_write_queue_size     = 0;
    int auto_increment              = 0;
    int next_increment_num          = 0;
    double auto_split_mb            = 0.0;
    int force_overwrite             = 0;
    int use_strftime                = 0;
    int fflush_interval_ms          = 100;
    int rotate                      = -1;
    int quiet                       = 0;

    // variables for inverted matching (e.g., logging all but some channels)
    int invert_channels             = 0;
    // XXX re-introduce regex
    string regex;

    size_t write_queue_size         = 0;
    int write_thread_exit_flag      = 0;

    // these members controlled by write thread
    size_t nevents                  = 0;
    size_t logsize                  = 0;
    size_t events_since_last_report = 0;
    u64    last_report_time         = 0;
    size_t last_report_logsize      = 0;
    u64    time0                    = 0;
    u64    last_fflush_time         = 0;

    size_t dropped_packets_count    = 0;
    u64    last_drop_report_utime   = 0;
    size_t last_drop_report_count   = 0;

    int num_splits                  = 0;

    Logger() {}

    void rotate_logfiles()
    {
        if (!quiet)
            printf("Rotating log files\n");

        // delete log files that have fallen off the end of the rotation
        string tomove = fname_prefix + "." + to_string(rotate-1);
        if (FileUtil::exists(tomove)) {
            if (0 != FileUtil::remove(tomove)) {
                fprintf(stderr, "ERROR! Unable to delete [%s]\n", tomove.c_str());
            }
        }

        // Rotate away any existing log files
        for (int file_num = rotate-1; file_num >= 0; file_num--) {
            string newname = fname_prefix + "." + to_string(file_num);
            string tomove  = fname_prefix + "." + to_string(file_num-1);
            if (FileUtil::exists(tomove)) {
                if (0 != FileUtil::rename(tomove, newname)) {
                    fprintf(stderr, "ERROR!  Unable to rotate [%s]\n", tomove.c_str());
                }
            }
        }
    }

    int open_logfile()
    {
        char tmp_path[PATH_MAX];

        // maybe run the filename through strftime
        if (use_strftime) {
            time_t now = time (NULL);
            strftime(tmp_path, sizeof(tmp_path),
                     input_fname.c_str(), localtime(&now));
            string new_prefix = tmp_path;

            // If auto-increment is enabled and the strftime-formatted filename
            // prefix has changed, then reset the auto-increment counter.
            if(auto_increment && fname_prefix != new_prefix)
                next_increment_num = 0;
            fname_prefix = std::move(new_prefix);
        } else {
            fname_prefix = input_fname;
        }

        if (auto_increment) {
            /* Loop through possible file names until we find one that doesn't
             * already exist.  This way, we never overwrite an existing file. */
            do {
                snprintf(tmp_path, sizeof(tmp_path), "%s.%02d",
                         fname_prefix.c_str(), next_increment_num);
                fname = tmp_path;
                next_increment_num++;
            } while(FileUtil::exists(fname));
        } else if(rotate > 0) {
            fname = fname_prefix + ".0";
        } else {
            fname = fname_prefix;
            if (!force_overwrite) {
                if (FileUtil::exists(fname)) {
                    fprintf(stderr, "Refusing to overwrite existing file \"%s\"\n", fname.c_str());
                    return 1;
                }
            }
        }

        // create directories if needed
        string dirpart = FileUtil::dirname(fname);
        if (!FileUtil::dirExists(dirpart))
            FileUtil::mkdirWithParents(dirpart, 0755);

        if(!quiet) {
            printf("Opening log file \"%s\"\n", fname.c_str());
        }

        // open output file in append mode if we're rotating log files, or write
        // mode if not.
        const char *logmode = (rotate > 0) ? "a" : "w";
        log = zcm_eventlog_create(fname.c_str(), logmode);
        if (!log) {
            perror("Error: fopen failed");
            return 1;
        }
        return 0;
    }

    void write_event(zcm_eventlog_event_t *le)
    {
        // Is it time to start a new logfile?
        int split_log = 0;
        if (auto_split_mb) {
            double logsize_mb = (double)logsize / (1 << 20);
            split_log = (logsize_mb > auto_split_mb);
        }
        if (_reset_logfile) {
            split_log = 1;
            _reset_logfile = 0;
        }

        if (split_log) {
            // Yes.  open up a new log file
            zcm_eventlog_destroy(log);
            if (rotate > 0)
                rotate_logfiles();
            if(0 != open_logfile())
                exit(1);
            num_splits++;
            logsize = 0;
            last_report_logsize = 0;
        }

        // Should the write thread exit?
        if (write_thread_exit_flag)
            return;

        // nope.  write the event to disk
        int64_t sz = sizeof(zcm_eventlog_event_t) + le->channellen + 1 + le->datalen;
        write_queue_size -= sz;

        if(0 != zcm_eventlog_write_event(log, le)) {
            static u64 last_spew_utime = 0;
            string reason = strerror(errno);
            u64 now = timestamp_now();
            if(now - last_spew_utime > 500000) {
                fprintf(stderr, "zcm_eventlog_write_event: %s\n", reason.c_str());
                last_spew_utime = now;
            }
            free(le);
            if(errno == ENOSPC) {
                exit(1);
            } else {
                return;
            }
        }
        if (fflush_interval_ms >= 0 &&
            (le->timestamp - last_fflush_time) > (u64)fflush_interval_ms*1000) {
            fflush(zcm_eventlog_get_fileptr(log));
            // Perform a full fsync operation after flush
#ifndef WIN32
            fdatasync(fileno(zcm_eventlog_get_fileptr(log)));
#endif
            last_fflush_time = le->timestamp;
        }

        // bookkeeping, cleanup
        int64_t offset_utime = le->timestamp - time0;
        nevents++;
        events_since_last_report ++;
        logsize += 4 + 8 + 8 + 4 + le->channellen + 4 + le->datalen;

        free(le);

        if (!quiet && (offset_utime - last_report_time > 1000000)) {
            double dt = (offset_utime - last_report_time)/1000000.0;

            double tps =  events_since_last_report / dt;
            double kbps = (logsize - last_report_logsize) / dt / 1024.0;
            printf("Summary: %s ti:%4" PRIi64 "sec Events: %-9" PRIi64 " ( %4" PRIi64 " MB )      TPS: %8.2f       KB/s: %8.2f\n",
                   fname.c_str(),
                   offset_utime / 1000000,
                   nevents, logsize/1048576,
                   tps, kbps);
            last_report_time = offset_utime;
            events_since_last_report = 0;
            last_report_logsize = logsize;
        }
    }

    void handler(const zcm_recv_buf_t *rbuf, const char *channel)
    {
        // XXX re-introduce invert channels
        // if (invert_channels) {
        //     if (g_regex_match(logger->regex, channel, (GRegexMatchFlags) 0, NULL))
        //         return;
        // }

        int channellen = strlen(channel);

        // check if the backlog of unwritten messages is too big.  If so, then
        // ignore this event
        size_t mem_sz = sizeof(zcm_eventlog_event_t) + channellen + 1 + rbuf->len;
        size_t mem_required = mem_sz + write_queue_size;

        if(mem_required > max_write_queue_size) {
            // XXX we should be detecting message drops and reporting
            // // can't write to logfile fast enough.  drop packet.
            // g_mutex_unlock(logger->mutex);

            // // maybe print an informational message to stdout
            // int64_t now = timestamp_now();
            // logger->dropped_packets_count ++;
            // int rc = logger->dropped_packets_count - logger->last_drop_report_count;

            // if(now - logger->last_drop_report_utime > 1000000 && rc > 0) {
            //     if(!logger->quiet)
            //         printf("Can't write to log fast enough.  Dropped %d packet%s\n",
            //                rc, rc==1?"":"s");
            //     logger->last_drop_report_utime = now;
            //     logger->last_drop_report_count = logger->dropped_packets_count;
            // }
            return;
        } else {
            write_queue_size = mem_required;
        }

        // queue up the message for writing to disk by the write thread
        zcm_eventlog_event_t *le = (zcm_eventlog_event_t*) malloc(mem_sz);
        memset(le, 0, mem_sz);

        le->timestamp = rbuf->utime;
        le->channellen = channellen;
        le->datalen = rbuf->len;
        // log_write_event will handle le.eventnum.

        le->channel = ((char*)le) + sizeof(zcm_eventlog_event_t);
        strcpy(le->channel, channel);
        le->data = le->channel + channellen + 1;
        assert((char*)le->data + rbuf->len == (char*)le + mem_sz);
        memcpy(le->data, rbuf->data, rbuf->len);

        write_event(le);
    }
};

static void message_handler(const zcm_recv_buf_t *rbuf, const char *channel, void *usr)
{
    Logger *logger = (Logger *) usr;
    logger->handler(rbuf, channel);
}

#ifdef USE_SIGHUP
static void sighup_handler (int signum)
{
    _reset_logfile = 1;
}
#endif

static void usage ()
{
    fprintf (stderr, "usage: zcm-logger [options] [FILE]\n"
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
            "      --flush-interval=MS    Flush the log file to disk every MS milliseconds.\n"
            "                             (default: 100)\n"
            "  -f, --force                Overwrite existing files\n"
            "  -h, --help                 Shows this help text and exits\n"
            "  -i, --increment            Automatically append a suffix to FILE\n"
            "                             such that the resulting filename does not\n"
            "                             already exist.  This option precludes -f and\n"
            "                             --rotate\n"
            "  -l, --zcm-url=URL          Log messages on the specified ZCM URL\n"
            "  -m, --max-unwritten-mb=SZ  Maximum size of received but unwritten\n"
            "                             messages to store in memory before dropping\n"
            "                             messages.  (default: 100 MB)\n"
            "      --rotate=NUM           When creating a new log file, rename existing files\n"
            "                             out of the way and always write to FILE.0.  If\n"
            "                             FILE.0 already exists, it is renamed to FILE.1.  If\n"
            "                             FILE.1 exists, it is renamed to FILE.2, etc.  If\n"
            "                             FILE.NUM exists, then it is deleted.  This option\n"
            "                             precludes -i.\n"
            "      --split-mb=N           Automatically start writing to a new log\n"
            "                             file once the log file exceeds N MB in size\n"
            "                             (can be fractional).  This option requires -i\n"
            "                             or --rotate.\n"
            "  -q, --quiet                Suppress normal output and only report errors.\n"
            "  -s, --strftime             Format FILE with strftime.\n"
            "  -v, --invert-channels      Invert channels.  Log everything that CHAN\n"
            "                             does not match.\n"
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
#ifndef WIN32
    setlinebuf (stdout);
#endif

    Logger logger{};

    // set some defaults
    string chan_regex = ".*";
    double max_write_queue_size_mb = DEFAULT_MAX_WRITE_QUEUE_SIZE_MB;

    string zcmurl = "";
    const char *optstring = "a:fic:shm:vu:q";
    int c;
    struct option long_opts[] = {
        { "split-mb", required_argument, 0, 'b' },
        { "channel", required_argument, 0, 'c' },
        { "force", no_argument, 0, 'f' },
        { "increment", required_argument, 0, 'i' },
        { "zcm-url", required_argument, 0, 'l' },
        { "max-unwritten-mb", required_argument, 0, 'm' },
        { "rotate", required_argument, 0, 'r' },
        { "strftime", required_argument, 0, 's' },
        { "quiet", no_argument, 0, 'q' },
        { "invert-channels", no_argument, 0, 'v' },
        { "flush-interval", required_argument, 0,'u'},
        { 0, 0, 0, 0 }
    };

    while ((c = getopt_long (argc, argv, optstring, long_opts, 0)) >= 0)
    {
        switch (c) {
            case 'b':
                logger.auto_split_mb = strtod(optarg, NULL);
                if(logger.auto_split_mb <= 0) {
                    usage();
                    return 1;
                }
                break;
            case 'f':
                logger.force_overwrite = 1;
                break;
            case 'c':
                chan_regex = optarg;
                break;
            case 'i':
                logger.auto_increment = 1;
                break;
            case 's':
                logger.use_strftime = 1;
                break;
            case 'l':
                zcmurl = optarg;
                break;
            case 'q':
                logger.quiet = 1;
                break;
            case 'v':
                logger.invert_channels = 1;
                break;
            case 'm':
                max_write_queue_size_mb = strtod(optarg, NULL);
                if(max_write_queue_size_mb <= 0) {
                    usage();
                    return 1;
                }
                break;
            case 'r':
                {
                  char* eptr = NULL;
                  logger.rotate = strtol(optarg, &eptr, 10);
                  if(*eptr) {
                      usage();
                      return 1;
                  }
                }
                break;
            case 'u':
              logger.fflush_interval_ms = atol(optarg);
              if(logger.fflush_interval_ms <= 0) {
                  usage();
                  return 1;
              }
              break;
            case 'h':
            default:
                usage();
                return 1;
        };
    }

    if (optind == argc) {
        logger.input_fname = "zcmlog-%Y-%m-%d";
        logger.auto_increment = 1;
        logger.use_strftime = 1;
    } else if (optind == argc - 1) {
        logger.input_fname = argv[optind];
    } else if (optind < argc-1) {
        usage ();
        return 1;
    }


    if(logger.auto_split_mb > 0 && !(logger.auto_increment || (logger.rotate > 0))) {
        fprintf(stderr, "ERROR.  --split-mb requires either --increment or --rotate\n");
        return 1;
    }
    if(logger.rotate > 0 && logger.auto_increment) {
        fprintf(stderr, "ERROR.  --increment and --rotate can't both be used\n");
        return 1;
    }


    logger.time0 = timestamp_now();
    logger.max_write_queue_size = (size_t)(max_write_queue_size_mb * (1 << 20));

    if(0 != logger.open_logfile())
        return 1;

    // create write thread
    logger.write_thread_exit_flag = 0;
    logger.write_queue_size = 0;

    // begin logging
    // XXX "ipc" is the hardcoded default: This should be an automatic
    //     by zcm_create whenever NULL is passed
    logger.zcm = zcm_create(zcmurl != "" ? zcmurl.c_str() : "ipc");
    if (!logger.zcm) {
        fprintf(stderr, "Couldn't initialize ZCM!");
        return 1;
    }

    if(logger.invert_channels) {
        // if inverting the channels, subscribe to everything and invert on the
        // callback
        zcm_subscribe(logger.zcm, ".*", message_handler, &logger);
        logger.regex = "^" + chan_regex + "$";
    } else {
        // otherwise, let ZCM handle the regex
        zcm_subscribe(logger.zcm, chan_regex.c_str(), message_handler, &logger);
    }

    // XXX ADD THIS
    //     _mainloop = g_main_loop_new (NULL, FALSE);
    //     signal_pipe_glib_quit_on_kill ();
    //     glib_mainloop_attach_zcm (logger.zcm);

#ifdef USE_SIGHUP
    signal(SIGHUP, sighup_handler);
#endif

    zcm_become(logger.zcm);

    // XXX ADD this
    //     // main loop
    //     g_main_loop_run (_mainloop);

    fprintf(stderr, "Logger exiting\n");
    return 0;
}
