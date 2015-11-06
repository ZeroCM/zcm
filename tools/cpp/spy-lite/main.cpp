#include "Common.hpp"
#include "MsgDisplay.hpp"
#include "TypeDb.hpp"
#include "MsgInfo.hpp"
#include "Debug.hpp"

#define SELECT_TIMEOUT 20000
#define ESCAPE_KEY 0x1B
#define DEL_KEY 0x7f

static volatile bool quit = false;

enum class DisplayMode {
    Overview, Decode,
};

struct SpyInfo
{
    SpyInfo(const char *path, bool debug)
    : typedb(path, debug)
    {
    }

    ~SpyInfo()
    {
        for (auto& it : minfomap)
            delete it.second;
    }

    MsgInfo *getCurrentMsginfo(const char **channel)
    {
        auto& ch = names[decode_index];
        MsgInfo **m = lookup(minfomap, ch);
        assert(m);
        if (channel)
            *channel = ch.c_str();
        return *m;
    }

    bool isValidChannelnum(size_t index)
    {
        return (0 <= index && index < names.size());
    }

    void addMessage(const char *channel, const zcm_recv_buf_t *rbuf);

    void display();
    void displayOverview();

    void handleKeyboard(char ch);
    void handleKeyboardOverview(char ch);
    void handleKeyboardDecode(char ch);

private:
    vector<string>                  names;
    unordered_map<string, MsgInfo*> minfomap;
    TypeDb typedb;

    mutex mut;

    DisplayMode mode = DisplayMode::Overview;
    bool is_selecting = false;

    int decode_index = 0;
    MsgInfo *decode_msg_info;
    const char *decode_msg_channel;
};

void SpyInfo::addMessage(const char *channel, const zcm_recv_buf_t *rbuf)
{
    unique_lock<mutex> lk(mut);

    MsgInfo *minfo = minfomap[channel];
    if (minfo == NULL) {
        minfo = new MsgInfo(typedb, channel);
        names.push_back(channel);
        std::sort(begin(names), end(names));
        minfomap[channel] = minfo;
    }
    minfo->addMessage(TimeUtil::utime(), rbuf);
}

void SpyInfo::display()
{
    unique_lock<mutex> lk(mut);

    switch (mode) {
        case DisplayMode::Overview: {
            displayOverview();
        } break;
        case DisplayMode::Decode: {
            decode_msg_info->display();
        } break;
        default:
            DEBUG(1, "ERR: unknown mode\n");
    }
}

void SpyInfo::displayOverview()
{
    printf("         %-28s\t%12s\t%8s\n", "Channel", "Num Messages", "Hz (ave)");
    printf("   ----------------------------------------------------------------\n");

    DEBUG(5, "start-loop\n");

    for (size_t i = 0; i < names.size(); i++) {
        auto& channel = names[i];
        MsgInfo **minfo = lookup(minfomap, channel);
        assert(minfo != NULL);
        float hz = (*minfo)->getHertz();
        printf("   %3zu)  %-28s\t%9" PRIu64 "\t%7.2f\n", i, channel.c_str(), (*minfo)->getNumMsgs(), hz);
    }

    printf("\n");

    if (is_selecting) {
        printf("   Decode channel: ");
        if (decode_index != -1)
            printf("%d", decode_index);
        fflush(stdout);
    }
}

void SpyInfo::handleKeyboardOverview(char ch)
{
    if (ch == '-') {
        is_selecting = true;
        decode_index = -1;
    } else if ('0' <= ch && ch <= '9') {
        // shortcut for single digit channels
        if (!is_selecting) {
            decode_index = ch - '0';
            if (isValidChannelnum(decode_index)) {
                decode_msg_info = getCurrentMsginfo(&decode_msg_channel);
                mode = DisplayMode::Decode;
            }
        } else {
            if (decode_index == -1) {
                decode_index = ch - '0';
            } else if (decode_index < 10000) {
                decode_index *= 10;
                decode_index += (ch - '0');
            }
        }
    } else if (ch == '\n') {
        if (is_selecting) {
            if (isValidChannelnum(decode_index)) {
                decode_msg_info = getCurrentMsginfo(&decode_msg_channel);
                mode = DisplayMode::Decode;
            }
            is_selecting = false;
        }
    } else if (ch == '\b' || ch == DEL_KEY) {
        if (is_selecting) {
            if (decode_index < 10)
                decode_index = -1;
            else
                decode_index /= 10;
        }
    } else {
        DEBUG(1, "INFO: unrecognized input: '%c' (0x%2x)\n", ch, ch);
    }
}

void SpyInfo::handleKeyboardDecode(char ch)
{
    MsgInfo& minfo = *decode_msg_info;
    size_t depth = minfo.getViewDepth();

    if (ch == ESCAPE_KEY) {
        if (depth > 0) {
            minfo.decViewDepth();
        } else {
            mode = DisplayMode::Overview;
        }
    } else if ('0' <= ch && ch <= '9') {
        // if number is pressed, set and increase sub-msg decoding depth
        size_t viewid = (ch - '0');
        if (depth < MSG_DISPLAY_RECUR_MAX) {
            minfo.incViewDepth(viewid);
        } else {
            DEBUG(1, "INFO: cannot recurse further: reached maximum depth of %d\n",
                  MSG_DISPLAY_RECUR_MAX);
        }
    } else {
        DEBUG(1, "INFO: unrecognized input: '%c' (0x%2x)\n", ch, ch);
    }
}

void SpyInfo::handleKeyboard(char ch)
{
    unique_lock<mutex> lk(mut);

    switch (mode) {
        case DisplayMode::Overview: handleKeyboardOverview(ch); break;
        case DisplayMode::Decode:   handleKeyboardDecode(ch);  break;
        default:
            DEBUG(1, "INFO: unrecognized keyboard mode: %d\n", (int)mode);
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

            spy->handleKeyboard(ch);

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

void printThreadFunc(SpyInfo *spy)
{
    static constexpr float hz = 20.0;
    static constexpr u64 period = 1000000 / hz;

    DEBUG(1, "INFO: %s: Starting\n", "print_thread");
    while (!quit) {
        usleep(period);

        clearscreen();
        printf("  **************************************************************************** \n");
        printf("  ******************************* ZCM-SPY-LITE ******************************* \n");
        printf("  **************************************************************************** \n");

        spy->display();

        // flush the stdout buffer (required since we use full buffering)
        fflush(stdout);
    }

    DEBUG(1, "INFO: %s: Ending\n", "print_thread");
}

//////////////////////////////////////////////////////////////////////
///////////////////////////// LCM HANDLER ////////////////////////////
//////////////////////////////////////////////////////////////////////

void handler_all_zcm (const zcm_recv_buf_t *rbuf,
                      const char *channel, void *arg)
{
    SpyInfo *spy = (SpyInfo *)arg;
    spy->addMessage(channel, rbuf);
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

    // start zcm
    zcm_t *zcm = zcm_create(NULL);
    if (!zcm) {
        DEBUG(1, "ERR: failed to create an zcm object!\n");
        return 1;
    }
    zcm_subscribe(zcm, ".*", handler_all_zcm, &spy);
    zcm_start(zcm);

    thread printThread {printThreadFunc, &spy};

    // use this thread as the keyboard thread
    keyboard_thread_func(&spy);

    // cleanup
    printThread.join();
    zcm_stop(zcm);

    DEBUG(1, "Exiting...\n");
    return 0;
}
