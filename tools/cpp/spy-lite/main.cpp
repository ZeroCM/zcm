#include "Common.hpp"
#include "MsgDisplay.hpp"
#include "TypeDb.hpp"

#define SELECT_TIMEOUT 20000
#define ESCAPE_KEY 0x1B
#define DEL_KEY 0x7f

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

#define QUEUE_PERIOD (4*1000*1000)   /* queue up to 4 sec of utimes */
#define QUEUE_SIZE   (400)           /* hold up to 400 utimes */

struct msg_info_t
{
    TypeDb& db;
    const char *channel;

    uint64_t queue[QUEUE_SIZE];
    int front = 0;  // front: the next index to dequeue
    int back = 0;   // back-1: the last index enqueued
    bool is_full = false;; // when front == back: the queue could be either full or empty (this disambiguates)

    int64_t hash = 0;
    const TypeMetadata *metadata = NULL;
    MsgDisplayState disp_state;
    void *last_msg = NULL;

    uint64_t num_msgs = 0;

    msg_info_t(TypeDb& db, const char *channel)
    : db(db), channel(channel)
    {}

    void _ensure_hash(int64_t hash)
    {
        if(this->hash == hash)
            return;

        // if this not the first message, warn user
        if(this->hash != 0) {
            DEBUG(1, "WRN: hash changed, searching for new lcmtype on channel %s\n", this->channel);
        }

        // cleanup old memory if needed
        if(this->metadata != NULL && this->last_msg != NULL) {
            this->metadata->info->decode_cleanup(this->last_msg);
            free(this->last_msg);
            this->last_msg = NULL;
        }

        this->hash = hash;
        this->metadata = db.getByHash(hash);
        if(this->metadata == NULL) {
            DEBUG(1, "WRN: failed to find lcmtype for hash: 0x%" PRIx64 "\n", hash);
            return;
        }
    }

    inline bool _is_empty()
    {
        return !this->is_full && (this->front == this->back);
    }

    inline int _get_size()
    {
        if(this->is_full)
            return QUEUE_SIZE;
        if(this->front <= this->back)
            return this->back - this->front;
        else /* this->front > this->back */
            return QUEUE_SIZE - (this->front - this->back);
    }

    inline void _dequeue()
    {
        /* assert not empty */
        assert(!_is_empty());
        ++this->front;
        this->front %= QUEUE_SIZE;
        this->is_full = false;
    }

    inline void _enqueue(uint64_t utime)
    {
        assert(!this->is_full);
        this->queue[this->back++] = utime;
        this->back %= QUEUE_SIZE;
        if(this->front == this->back)
            this->is_full = true;
    }

    uint64_t _latest_utime()
    {
        assert(!_is_empty());

        int i = this->back - 1;
        if(i < 0)
            i = QUEUE_SIZE - 1;

        return this->queue[i];
    }

    uint64_t _oldest_utime()
    {
        assert(!_is_empty());
        return this->queue[this->front];
    }

    void _remove_old()
    {
        uint64_t oldest_allowed = TimeUtil::utime() - QUEUE_PERIOD;

        // discard old messages
        while(!_is_empty()) {
            uint64_t last = this->queue[this->front];
            if(last < oldest_allowed)
                _dequeue();
            else
                break;
        }
    }

    void add_msg(uint64_t utime, const zcm_recv_buf_t *rbuf)
    {
        if(this->is_full)
            _dequeue();
        _enqueue(utime);

        this->num_msgs++;

        /* decode the data */
        int64_t hash;
        __int64_t_decode_array(rbuf->data, 0, rbuf->data_size, &hash, 1);
        _ensure_hash(hash);

        if(this->metadata != NULL) {

            // do we need to allocate memory for 'last_msg' ?
            if(this->last_msg == NULL) {
                size_t sz = this->metadata->info->struct_size();
                this->last_msg = malloc(sz);
            } else {
                this->metadata->info->decode_cleanup(this->last_msg);
            }

            // actually decode it
            this->metadata->info->decode(rbuf->data, 0, rbuf->data_size, this->last_msg);

            DEBUG(1, "INFO: successful decode on %s\n", this->channel);
        }
    }

    float get_hz()
    {
        _remove_old();
        if (_is_empty())
            return 0.0;

        int sz = _get_size();
        uint64_t oldest = _oldest_utime();
        uint64_t latest = _latest_utime();
        uint64_t dt = latest - oldest;
        if(dt == 0.0)
            return 0.0;

        return (float) sz / ((float) dt / 1000000.0);
    }
};

enum display_mode { MODE_OVERVIEW, MODE_DECODE };
struct spyinfo_t
{
    vector<string>                     names_array;
    unordered_map<string, msg_info_t*> minfo_hashtbl;
    TypeDb type_db;
    pthread_mutex_t mutex;
    float display_hz = 10.0;

    enum display_mode mode = MODE_OVERVIEW;
    bool is_selecting = false;

    int decode_index = 0;
    msg_info_t *decode_msg_info;
    const char *decode_msg_channel;

    spyinfo_t(const char *path, bool debug)
    : type_db(path, debug)
    {}

    ~spyinfo_t()
    {
        for (auto& it : minfo_hashtbl)
            delete it.second;
    }
};

static msg_info_t *get_current_msg_info(spyinfo_t *spy, const char **channel)
{
    auto& ch = spy->names_array[spy->decode_index];
    msg_info_t **minfo = lookup(spy->minfo_hashtbl, ch);
    assert(minfo != NULL);
    if (channel != NULL) *channel = ch.c_str();
    return *minfo;
}

static int is_valid_channel_num(spyinfo_t *spy, int index)
{
    return (0 <= index && index < (int)spy->names_array.size());
}

static void keyboard_handle_overview(spyinfo_t *spy, char ch)
{
    if(ch == '-') {
        spy->is_selecting = true;
        spy->decode_index = -1;
    } else if('0' <= ch && ch <= '9') {
        // shortcut for single digit channels
        if(!spy->is_selecting) {
            spy->decode_index = ch - '0';
            if(is_valid_channel_num(spy, spy->decode_index)) {
                spy->decode_msg_info = get_current_msg_info(spy, &spy->decode_msg_channel);
                spy->mode = MODE_DECODE;
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
            if(is_valid_channel_num(spy, spy->decode_index)) {
                spy->decode_msg_info = get_current_msg_info(spy, &spy->decode_msg_channel);
                spy->mode = MODE_DECODE;
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

static void keyboard_handle_decode(spyinfo_t *spy, char ch)
{
    auto& ds = spy->decode_msg_info->disp_state;

    if(ch == ESCAPE_KEY) {
        if(ds.cur_depth > 0)
            ds.cur_depth--;
        else
            spy->mode = MODE_OVERVIEW;
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
    spyinfo_t *spy = (spyinfo_t *)arg;

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
                    case MODE_OVERVIEW: keyboard_handle_overview(spy, ch); break;
                    case MODE_DECODE:   keyboard_handle_decode(spy, ch);  break;
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

static void display_overview(spyinfo_t *spy)
{
    printf("         %-28s\t%12s\t%8s\n", "Channel", "Num Messages", "Hz (ave)");
    printf("   ----------------------------------------------------------------\n");

    DEBUG(5, "start-loop\n");

    for (size_t i = 0; i < spy->names_array.size(); i++) {
        auto& channel = spy->names_array[i];
        msg_info_t **minfo = lookup(spy->minfo_hashtbl, channel);
        assert(minfo != NULL);
        float hz = (*minfo)->get_hz();
        printf("   %3d)  %-28s\t%9" PRIu64 "\t%7.2f\n", (int)i, channel.c_str(), (*minfo)->num_msgs, hz);
    }

    printf("\n");

    if(spy->is_selecting) {
        printf("   Decode channel: ");
        if(spy->decode_index != -1)
            printf("%d", spy->decode_index);
        fflush(stdout);
    }
}

static void display_decode(spyinfo_t *spy)
{
    msg_info_t *minfo = spy->decode_msg_info;
    const char *channel = spy->decode_msg_channel;

    const char *name = NULL;
    i64 hash = 0;
    if (minfo->metadata != NULL) {
        name = minfo->metadata->name.c_str();
        hash = minfo->metadata->info->get_hash();
    }

    printf("         Decoding %s (%s) %" PRIu64 ":\n", channel, name, (uint64_t) hash);

    if(minfo->last_msg != NULL)
        msg_display(spy->type_db, *minfo->metadata, minfo->last_msg,  minfo->disp_state);
}

void *print_thread_func(void *arg)
{
    spyinfo_t *spy = (spyinfo_t *)arg;

    const double MAX_FREQ = 100.0;
    int period;
    float hz = 10.0;

    if (spy->display_hz <= 0) {
        DEBUG(1, "WRN: Invalid Display Hz, defaulting to %3.3fHz\n", hz);
    } else if (spy->display_hz > MAX_FREQ) {
        DEBUG(1, "WRN: Invalid Display Hz, defaulting to %1.0f Hz\n", MAX_FREQ);
        hz = MAX_FREQ;
    } else {
        hz = spy->display_hz;
    }

    period = 1000000 / hz;

    DEBUG(1, "INFO: %s: Starting\n", "print_thread");
    while (!quit) {
        usleep(period);

        clearscreen();
        printf("  **************************************************************************** \n");
        printf("  ************************** LCM-SPY (lite) [%3.1f Hz] ************************ \n", hz);
        printf("  **************************************************************************** \n");

        pthread_mutex_lock(&spy->mutex);
        {
            switch(spy->mode) {

                case MODE_OVERVIEW:
                    display_overview(spy);
                    break;

                case MODE_DECODE:
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
    spyinfo_t *spy = (spyinfo_t *)arg;
    msg_info_t *minfo;
    uint64_t utime = TimeUtil::utime();

    pthread_mutex_lock(&spy->mutex);
    {
        auto **minfo_ = lookup(spy->minfo_hashtbl, string(channel));
        if (minfo_ == NULL) {
            minfo = new msg_info_t(spy->type_db, channel);
            spy->names_array.push_back(channel);
            std::sort(begin(spy->names_array), end(spy->names_array));
            spy->minfo_hashtbl[channel] = minfo;
        } else {
            minfo = *minfo_;
        }
        minfo->add_msg(utime, rbuf);
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

    spyinfo_t spy {spy_lite_path, debug};
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
