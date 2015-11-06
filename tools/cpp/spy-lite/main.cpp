#include "Common.hpp"
#include "MsgDisplay.hpp"
#include "TypeDb.hpp"
#include "ExpiringQueue.hpp"

#define SELECT_TIMEOUT 20000
#define ESCAPE_KEY 0x1B
#define DEL_KEY 0x7f
#define QUEUE_PERIOD (4*1000*1000)   /* queue up to 4 sec of utimes */
#define QUEUE_SIZE   (400)           /* hold up to 400 utimes */

static volatile bool quit = false;

#define DEBUG_LEVEL 2  /* 0=nothing, higher values mean more verbosity */
#define DEBUG_FILENAME "/tmp/spy-lite-debug.log"
static FILE *DEBUG_FILE = NULL;
static void DEBUG_INIT(void) { DEBUG_FILE = fopen(DEBUG_FILENAME, "w"); }

static void DEBUG(int level, const char *fmt, ...)
{
    if(!DEBUG_LEVEL || level > DEBUG_LEVEL)
        return;

    va_list vargs;
    va_start(vargs, fmt);
    vfprintf(DEBUG_FILE, fmt, vargs);
    va_end(vargs);

    fflush(DEBUG_FILE);
}

//////////////////////////////////////////////////////////////////////
//////////////////////////////// Structs /////////////////////////////
//////////////////////////////////////////////////////////////////////

class MsgInfo
{
public:
    MsgInfo(TypeDb& db, const char *channel)
    : db(db), channel(channel)
    {}

    void addMessage(u64 utime, const zcm_recv_buf_t *rbuf);
    float getHertz();
    u64 getNumMsgs() { return num_msgs; }

private:
    void ensureHash(i64 hash);
    u64 latestUtime();
    u64 oldestUtime();
    void removeOld();

private:
    TypeDb& db;
    const char *channel;

    ExpiringQueue<u64, QUEUE_SIZE> queue;
    i64 hash = 0;
    u64 num_msgs = 0;

public: // HAX
    MsgDisplayState disp_state;
    const TypeMetadata *metadata = NULL;
    void *last_msg = NULL;
};

void MsgInfo::ensureHash(i64 h)
{
    if (hash == h)
        return;

    // if this not the first message, warn user
    if (hash != 0) {
        DEBUG(1, "WRN: hash changed, searching for new lcmtype on channel %s\n", this->channel);
    }

    // cleanup old memory if needed
    if (metadata && last_msg) {
        metadata->info->decode_cleanup(last_msg);
        free(last_msg);
        last_msg = NULL;
    }

    hash = h;
    metadata = db.getByHash(h);
    if (metadata == NULL) {
        DEBUG(1, "WRN: failed to find lcmtype for hash: 0x%" PRIx64 "\n", hash);
        return;
    }
}

u64 MsgInfo::latestUtime()
{
    return queue.last();
}

u64 MsgInfo::oldestUtime()
{
    return queue.first();
}

void MsgInfo::removeOld()
{
    u64 oldest_allowed = TimeUtil::utime() - QUEUE_PERIOD;
    while (!queue.isEmpty() && queue.first() < oldest_allowed)
        queue.dequeue();
}

void MsgInfo::addMessage(u64 utime, const zcm_recv_buf_t *rbuf)
{
    if (queue.isFull())
        queue.dequeue();
    queue.enqueue(utime);

    num_msgs++;

    /* decode the data */
    i64 hash;
    __int64_t_decode_array(rbuf->data, 0, rbuf->data_size, &hash, 1);
    ensureHash(hash);

    if (metadata) {
        // do we need to allocate memory for 'last_msg' ?
        if (!last_msg) {
            size_t sz = metadata->info->struct_size();
            last_msg = malloc(sz);
        } else {
            metadata->info->decode_cleanup(last_msg);
        }

        // actually decode it
        metadata->info->decode(rbuf->data, 0, rbuf->data_size, last_msg);

        DEBUG(1, "INFO: successful decode on %s\n", channel);
    }
}

float MsgInfo::getHertz()
{
    removeOld();
    if (queue.isEmpty())
        return 0.0;

    int sz = queue.getSize();
    u64 oldest = oldestUtime();
    u64 latest = latestUtime();
    u64 dt = latest - oldest;
    if(dt == 0.0)
        return 0.0;

    return (float) sz / ((float) dt / 1000000.0);
}

struct SpyInfo
{
    typedef enum {
        MODE_OVERVIEW,
        MODE_DECODE
    } DisplayMode_t;

    SpyInfo(const char *path, bool debug)
    : typedb(path, debug)
    {}

    ~SpyInfo()
    {
        for (auto& it : minfo)
            delete it.second;
    }

    MsgInfo *getCurrentMsginfo(const char **channel)
    {
        auto& ch = names[decode_index];
        MsgInfo **m = lookup(minfo, ch);
        assert(m);
        if (channel)
            *channel = ch.c_str();
        return *m;
    }

    bool isValidChannelnum(size_t index)
    {
        return (0 <= index && index < names.size());
    }

    vector<string>                  names;
    unordered_map<string, MsgInfo*> minfo;
    TypeDb typedb;

    pthread_mutex_t mutex;

    DisplayMode_t mode = MODE_OVERVIEW;
    bool is_selecting = false;

    int decode_index = 0;
    MsgInfo *decode_msg_info;
    const char *decode_msg_channel;
};

static void keyboard_handle_overview(SpyInfo *spy, char ch)
{
    if(ch == '-') {
        spy->is_selecting = true;
        spy->decode_index = -1;
    } else if('0' <= ch && ch <= '9') {
        // shortcut for single digit channels
        if(!spy->is_selecting) {
            spy->decode_index = ch - '0';
            if(spy->isValidChannelnum(spy->decode_index)) {
                spy->decode_msg_info = spy->getCurrentMsginfo(&spy->decode_msg_channel);
                spy->mode = SpyInfo::MODE_DECODE;
            }
        } else {
            if(spy->decode_index == -1) {
                spy->decode_index = ch - '0';
            } else if(spy->decode_index < 10000) {
                spy->decode_index *= 10;
                spy->decode_index += (ch - '0');
            }
        }
    } else if(ch == '\n') {
        if(spy->is_selecting) {
            if(spy->isValidChannelnum(spy->decode_index)) {
                spy->decode_msg_info = spy->getCurrentMsginfo(&spy->decode_msg_channel);
                spy->mode = SpyInfo::MODE_DECODE;
            }
            spy->is_selecting = false;
        }
    } else if(ch == '\b' || ch == DEL_KEY) {
        if(spy->is_selecting) {
            if(spy->decode_index < 10)
                spy->decode_index = -1;
            else
                spy->decode_index /= 10;
        }
    } else {
        DEBUG(1, "INFO: unrecognized input: '%c' (0x%2x)\n", ch, ch);
    }
}

static void keyboard_handle_decode(SpyInfo *spy, char ch)
{
    auto& ds = spy->decode_msg_info->disp_state;

    if(ch == ESCAPE_KEY) {
        if(ds.cur_depth > 0)
            ds.cur_depth--;
        else
            spy->mode = SpyInfo::MODE_OVERVIEW;
    } else if('0' <= ch && ch <= '9') {
        // if number is pressed, set and increase sub-msg decoding depth
        if(ds.cur_depth < MSG_DISPLAY_RECUR_MAX) {
            ds.recur_table[ds.cur_depth++] = (ch - '0');
        } else {
            DEBUG(1, "INFO: cannot recurse further: reached maximum depth of %d\n",
                  MSG_DISPLAY_RECUR_MAX);
        }
    } else {
        DEBUG(1, "INFO: unrecognized input: '%c' (0x%2x)\n", ch, ch);
    }
}

void *keyboard_thread_func(void *arg)
{
    SpyInfo *spy = (SpyInfo *)arg;

    struct termios old = {0};
    if (tcgetattr(0, &old) < 0)
        perror("tcsetattr()");

    struct termios newt = old;
    newt.c_lflag &= ~ICANON;
    newt.c_lflag &= ~ECHO;
    newt.c_cc[VMIN] = 1;
    newt.c_cc[VTIME] = 0;
    if (tcsetattr(0, TCSANOW, &newt) < 0)
        perror("tcsetattr ICANON");

    char ch;
    while(!quit) {
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(0, &fds);

        struct timeval timeout = { 0, SELECT_TIMEOUT };
        int status = select(1, &fds, 0, 0, &timeout);

        if(quit)
            break;

        if(status != 0 && FD_ISSET(0, &fds)) {

            if(read(0, &ch, 1) < 0)
                perror ("read()");

            pthread_mutex_lock(&spy->mutex);
            {
                switch(spy->mode) {
                    case SpyInfo::MODE_OVERVIEW: keyboard_handle_overview(spy, ch); break;
                    case SpyInfo::MODE_DECODE:   keyboard_handle_decode(spy, ch);  break;
                    default:
                        DEBUG(1, "INFO: unrecognized keyboard mode: %d\n", spy->mode);
                }
            }
            pthread_mutex_unlock(&spy->mutex);

        } else {
            DEBUG(4, "INFO: keyboard_thread_func select() timeout\n");
        }
    }

    if (tcsetattr(0, TCSADRAIN, &old) < 0)
        perror ("tcsetattr ~ICANON");

    return NULL;
}
//////////////////////////////////////////////////////////////////////
////////////////////////// Helper Functions //////////////////////////
//////////////////////////////////////////////////////////////////////

void clearscreen()
{
    // clear
    printf("\033[2J");

    // move cursor to (0, 0)
    printf("\033[0;0H");
}

//////////////////////////////////////////////////////////////////////
//////////////////////////// Print Thread ////////////////////////////
//////////////////////////////////////////////////////////////////////

static void display_overview(SpyInfo *spy)
{
    printf("         %-28s\t%12s\t%8s\n", "Channel", "Num Messages", "Hz (ave)");
    printf("   ----------------------------------------------------------------\n");

    DEBUG(5, "start-loop\n");

    for (size_t i = 0; i < spy->names.size(); i++) {
        auto& channel = spy->names[i];
        MsgInfo **minfo = lookup(spy->minfo, channel);
        assert(minfo != NULL);
        float hz = (*minfo)->getHertz();
        printf("   %3d)  %-28s\t%9" PRIu64 "\t%7.2f\n", (int)i, channel.c_str(), (*minfo)->getNumMsgs(), hz);
    }

    printf("\n");

    if(spy->is_selecting) {
        printf("   Decode channel: ");
        if(spy->decode_index != -1)
            printf("%d", spy->decode_index);
        fflush(stdout);
    }
}

static void display_decode(SpyInfo *spy)
{
    MsgInfo *minfo = spy->decode_msg_info;
    const char *channel = spy->decode_msg_channel;

    const char *name = NULL;
    i64 hash = 0;
    if (minfo->metadata != NULL) {
        name = minfo->metadata->name.c_str();
        hash = minfo->metadata->info->get_hash();
    }

    printf("         Decoding %s (%s) %" PRIu64 ":\n", channel, name, (uint64_t) hash);

    if(minfo->last_msg != NULL)
        msg_display(spy->typedb, *minfo->metadata, minfo->last_msg,  minfo->disp_state);
}

void *print_thread_func(void *arg)
{
    SpyInfo *spy = (SpyInfo *)arg;

    const float hz = 20.0;
    const uint64_t period = 1000000 / hz;

    DEBUG(1, "INFO: %s: Starting\n", "print_thread");
    while (!quit) {
        usleep(period);

        clearscreen();
        printf("  **************************************************************************** \n");
        printf("  ******************************* ZCM-SPY-LITE ******************************* \n");
        printf("  **************************************************************************** \n");

        pthread_mutex_lock(&spy->mutex);
        {
            switch(spy->mode) {

                case SpyInfo::MODE_OVERVIEW:
                    display_overview(spy);
                    break;

                case SpyInfo::MODE_DECODE:
                    display_decode(spy);
                    break;

                default:
                    DEBUG(1, "ERR: unknown mode\n");
            }
        }
        pthread_mutex_unlock(&spy->mutex);

        // flush the stdout buffer (required since we use full buffering)
        fflush(stdout);
    }

    DEBUG(1, "INFO: %s: Ending\n", "print_thread");

    return NULL;
}

//////////////////////////////////////////////////////////////////////
///////////////////////////// LCM HANDLER ////////////////////////////
//////////////////////////////////////////////////////////////////////

void handler_all_zcm (const zcm_recv_buf_t *rbuf,
                      const char *channel, void *arg)
{
    SpyInfo *spy = (SpyInfo *)arg;
    MsgInfo *minfo;
    uint64_t utime = TimeUtil::utime();

    pthread_mutex_lock(&spy->mutex);
    {
        auto **minfo_ = lookup(spy->minfo, string(channel));
        if (minfo_ == NULL) {
            minfo = new MsgInfo(spy->typedb, channel);
            spy->names.push_back(channel);
            std::sort(begin(spy->names), end(spy->names));
            spy->minfo[channel] = minfo;
        } else {
            minfo = *minfo_;
        }
        minfo->addMessage(utime, rbuf);
    }
    pthread_mutex_unlock(&spy->mutex);
}

//////////////////////////////////////////////////////////////////////
////////////////////////////////// MAIN //////////////////////////////
//////////////////////////////////////////////////////////////////////

static void sighandler(int s)
{
    switch(s) {
        case SIGQUIT:
        case SIGINT:
        case SIGTERM:
            DEBUG(1, "Caught signal...\n");
            quit = true;
            break;
        default:
            DEBUG(1, "WRN: unrecognized signal fired\n");
            break;
    }
}

int main(int argc, char *argv[])
{
    DEBUG_INIT();
    bool debug = false;

    // XXX get opt
    if(argc > 1 && strcmp(argv[1], "--debug") == 0)
        debug = true;

    // get the zcmtypes.so from ZCM_SPY_LITE_PATH
    const char *spy_lite_path = getenv("ZCM_SPY_LITE_PATH");
    if (debug)
        printf("zcm_spy_lite_path='%s'\n", spy_lite_path);
    if (spy_lite_path == NULL) {
        fprintf(stderr, "ERR: invalid $ZCM_SPY_LITE_PATH\n");
        return 1;
    }

    SpyInfo spy {spy_lite_path, debug};
    if (debug)
        exit(0);

    signal(SIGINT, sighandler);
    signal(SIGQUIT, sighandler);
    signal(SIGTERM, sighandler);

    // configure stdout buffering: use FULL buffering to avoid flickering
    setvbuf(stdout, NULL, _IOFBF, 2048);

    // start threads
    pthread_mutex_init(&spy.mutex, NULL);

    // start zcm
    zcm_t *zcm = zcm_create(NULL);
    if (!zcm) {
        DEBUG(1, "ERR: failed to create an zcm object!\n");
        return 1;
    }
    zcm_subscribe(zcm, ".*", handler_all_zcm, &spy);
    zcm_start(zcm);


    pthread_t print_thread;
    if (pthread_create(&print_thread, NULL, print_thread_func, &spy)) {
        printf("ERR: %s: Failed to start thread\n", "print_thread");
        exit(-1);
    }

    // use this thread as the keyboard thread
    keyboard_thread_func(&spy);

    // cleanup
    pthread_join(print_thread, NULL);
    zcm_stop(zcm);
    pthread_mutex_destroy(&spy.mutex);

    DEBUG(1, "Exiting...\n");
    return 0;
}
