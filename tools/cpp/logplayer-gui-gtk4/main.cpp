#include <atomic>
#include <cctype>
#include <chrono>
#include <condition_variable>
#include <fstream>
#include <getopt.h>
#include <gtk/gtk.h>
#include <iomanip>
#include <iostream>
#include <limits>
#include <mutex>
#include <thread>
#include <map>

#include <zcm/zcm-cpp.hpp>

#include "util/TimeUtil.hpp"
#include "util/StringUtil.hpp"
#include "zcm/util/Filter.hpp"

using namespace std;

static atomic_int done {0};

// Helper function for string operations
static bool startsWith(const string& str, const string& prefix) {
    return str.length() >= prefix.length() && 
           str.substr(0, prefix.length()) == prefix;
}

static double mathMap(double a, double inMin, double inMax, double outMin, double outMax)
{
    return (a - inMin) * (outMax - outMin) / (inMax - inMin) + outMin;
}

struct Args
{
    string filename = "";
    string zcmUrlOut = "";
    bool highAccuracyMode = false;
    bool verbose = false;
    bool exitWhenDone = false;
    bool playOnStart = false;

    bool init(int argc, char *argv[])
    {
        struct option long_opts[] = {
            { "help",                no_argument, 0, 'h' },
            { "zcm-url",       required_argument, 0, 'u' },
            { "high-accuracy",       no_argument, 0, 'a' },
            { "verbose",             no_argument, 0, 'v' },
            { "exit-when-done",      no_argument, 0, 'e' },
            { "play-on-start",       no_argument, 0, 'p' },
            { 0, 0, 0, 0 }
        };

        int c;
        while ((c = getopt_long(argc, argv, "hu:avep", long_opts, 0)) >= 0) {
            switch (c) {
                case 'u':        zcmUrlOut = string(optarg);       break;
                case 'a': highAccuracyMode = true;                 break;
                case 'v':          verbose = true;                 break;
                case 'e':     exitWhenDone = true;                 break;
                case 'p':      playOnStart = true;                 break;
                case 'h': default: usage(); return false;
            };
        }

        if (optind == argc - 1)
            filename = string(argv[optind]);

        return true;
    }

    void usage()
    {
        cerr << "usage: zcm-logplayer-gui [options] [FILE]" << endl
             << "" << endl
             << "    Reads packets from an ZCM log file and publishes them to a " << endl
             << "    ZCM transport with a GUI for control of playback." << endl
             << "" << endl
             << "Options:" << endl
             << "" << endl
             << "  -u, --zcm-url=URL      Play logged messages on the specified ZCM URL." << endl
             << "  -a, --high-accuracy    Enable extremely accurate publish timing." << endl
             << "                         Note that enabling this feature will probably consume" << endl
             << "                         a full CPU so logplayer can bypass the OS scheduler" << endl
             << "                         with busy waits in between messages." << endl
             << "  -v, --verbose          Print information about each packet." << endl
             << "  -h, --help             Shows some help text and exits." << endl
             << endl;
    }
};

enum {
   LOG_CHAN_COLUMN,
   PLAY_CHAN_COLUMN,
   ENABLED_COLUMN,
   NUM_COLUMNS
};

struct LogPlayer
{
    Args args;
    zcm::LogFile *zcmIn  = nullptr;
    zcm::ZCM     *zcmOut = nullptr;

    mutex windowLk;
    GtkWidget *window;
    GtkWidget *lblLogName;
    GtkWidget *btnPlay;
    GtkWidget *btnStep;
    GtkWidget *btnSlower;
    GtkWidget *lblSpeedTarget;
    GtkWidget *btnFaster;
    GtkWidget *lblCurrTime;
    GtkWidget *lblCurrSpeed;
    GtkWidget *sclMacroScrub;
    GtkWidget *sclMicroScrub;
    GtkWidget *menuScrub;
    GtkWidget *tblData;
    GtkWidget *btnToggle;
    GtkWidget *txtPrefix;

    // Event controllers
    GtkEventController *keyController;
    GtkGestureClick *logNameClickGesture;
    GtkGestureClick *macroScrubClickGesture;
    GtkGestureClick *microScrubClickGesture;

    uint64_t totalTimeUs;
    uint64_t firstMsgUtime;

    double microScrubCurr = 0;
    double microScrubMin = 0;
    double microScrubMax = 1;
    bool microScrubWasPlayingOnStart;
    bool microScrubIsDragging = false;
    bool macroScrubIsDragging = false;

    bool bDown = false;

    mutex redrawLk;
    condition_variable redrawCv;
    bool redrawNow;

    mutex zcmLk;
    condition_variable zcmCv;
    bool isPlaying = false;
    double speedTarget = 1.0;
    double currSpeed = 0.0;
    int ignoreMicroScrubEvts = 0;
    int ignoreMacroScrubEvts = 0;
    uint64_t microPivotTimeUs;
    uint64_t currMsgUtime = 0;
    uint64_t requestTimeUs = numeric_limits<uint64_t>::max();
    bool stepRequest = false;
    string stepPrefix;

    static constexpr int GDK_LEFT_CLICK = 1;
    static constexpr int GDK_RIGHT_CLICK = 3;

    enum class BookmarkType
    {
        PLAIN,
        LREPEAT,
        RREPEAT,
        NUM_BOOKMARK_TYPES
    };
    static BookmarkType bookmarkTypeFromSting(const string& type)
    {
        if (type == "PLAIN") return BookmarkType::PLAIN;
        if (type == "LREPEAT") return BookmarkType::LREPEAT;
        if (type == "RREPEAT") return BookmarkType::RREPEAT;
        return BookmarkType::NUM_BOOKMARK_TYPES;
    }
    static string bookmarkTypeToSting(BookmarkType type)
    {
        switch (type) {
            case BookmarkType::PLAIN: return "PLAIN";
            case BookmarkType::LREPEAT: return "LREPEAT";
            case BookmarkType::RREPEAT: return "RREPEAT";
            case BookmarkType::NUM_BOOKMARK_TYPES: assert(false && "Shouldn't be possible");
        }
        assert(false && "Shouldn't be possible");
        return "";
    }
    struct Bookmark
    {
        BookmarkType type;
        double logTimePerc;
        bool addedToGui;
        Bookmark(BookmarkType type, double logTimePerc) :
            type(type), logTimePerc(logTimePerc), addedToGui(false)
        {}
    };
    vector<Bookmark> bookmarks;

    struct ChannelInfo
    {
        string pubChannel;
        bool enabled;
        bool addedToGui;
    };
    map<string, ChannelInfo> channelMap;

    LogPlayer() {}

    ~LogPlayer()
    {
        if (zcmIn) delete zcmIn;
        if (zcmOut) delete zcmOut;
    }

    void loadPreferences()
    {
        ifstream f(".zcm-logplayer-gui");
        if (!f.good()) return;

        string line;
        while (getline(f, line)) {
            if (line.size() == 0) continue;
            if (line[0] == '#') continue;

            if (startsWith(line, "bookmark ")) {
                vector<string> toks = StringUtil::split(line, ' ');
                if (toks.size() != 3) continue;
                BookmarkType type = bookmarkTypeFromSting(toks[1]);
                if (type == BookmarkType::NUM_BOOKMARK_TYPES) continue;
                double logTimePerc = stod(toks[2]);
                bookmarks.emplace_back(type, logTimePerc);
            }

            if (startsWith(line, "channel ")) {
                vector<string> toks = StringUtil::split(line, ' ');
                if (toks.size() != 4) continue;
                string logChan = toks[1];
                string pubChan = toks[2];
                bool enabled = (toks[3] == "true");
                ChannelInfo& ci = channelMap[logChan];
                ci.pubChannel = pubChan;
                ci.enabled = enabled;
            }
        }
    }

    void savePreferences()
    {
        ofstream f(".zcm-logplayer-gui", ios::trunc);
        if (!f.good()) {
            cerr << "Unable to save preferences" << endl;
            return;
        }

        f << "# Zcm Logplayer GUI preferences" << endl;

        {
            unique_lock<mutex> lk(windowLk);
            for (const auto &b : bookmarks) {
                f << "bookmark " << bookmarkTypeToSting(b.type) << " "
                  << fixed << setprecision(10) << b.logTimePerc << endl;
            }

            for (const auto &p : channelMap) {
                f << "channel " << p.first << " " << p.second.pubChannel
                  << " " << (p.second.enabled ? "true" : "false") << endl;
            }
        }
    }

    void wakeup()
    {
        unique_lock<mutex> lk(redrawLk);
        redrawNow = true;
        redrawCv.notify_all();
    }

    void addBookmark(BookmarkType type, double logTimePerc)
    {
        unique_lock<mutex> lk(windowLk);
        bookmarks.emplace_back(type, logTimePerc);
        if (zcmIn && zcmIn->good()) {
            size_t idx = bookmarks.size() - 1;
            gtk_scale_add_mark(GTK_SCALE(sclMacroScrub),
                               logTimePerc, GTK_POS_TOP,
                               to_string(idx).c_str());
            bookmarks[idx].addedToGui = true;
        }
    }

    bool toggleChannelPublish(const string& logChannel)
    {
        unique_lock<mutex> lk(windowLk);
        auto itr = channelMap.find(logChannel);
        if (itr == channelMap.end()) return false;

        ChannelInfo& ci = itr->second;
        ci.enabled = !ci.enabled;

        #pragma GCC diagnostic push
        #pragma GCC diagnostic ignored "-Wdeprecated-declarations"
        GtkTreeModel *model = gtk_tree_view_get_model(GTK_TREE_VIEW(tblData));
        GtkTreeIter iter;
        if (gtk_tree_model_get_iter_first(model, &iter)) {
            do {
                char *logChan;
                gtk_tree_model_get(model, &iter, LOG_CHAN_COLUMN, &logChan, -1);
                if (string(logChan) == logChannel) {
                    gtk_list_store_set(GTK_LIST_STORE(model), &iter,
                                       ENABLED_COLUMN, ci.enabled, -1);
                    g_free(logChan);
                    break;
                }
                g_free(logChan);
            } while (gtk_tree_model_iter_next(model, &iter));
        }
        #pragma GCC diagnostic pop

        return true;
    }

    bool initializeZcm()
    {
        if (args.filename.empty()) return false;

        zcmIn = new zcm::LogFile(args.filename, "r");
        if (!zcmIn->good()) {
            cerr << "Unable to open zcm log file" << endl;
            return false;
        }

        const zcm::LogEvent *evt;
        evt = zcmIn->readNextEvent();
        if (evt == nullptr) {
            cerr << "Log file appears to be empty" << endl;
            return false;
        }
        firstMsgUtime = evt->timestamp;

        uint64_t lastMsgUtime = firstMsgUtime;
        do {
            lastMsgUtime = evt->timestamp;
            string channel = evt->channel;
            if (channelMap.find(channel) == channelMap.end()) {
                ChannelInfo& ci = channelMap[channel];
                ci.pubChannel = channel;
                ci.enabled = true;
                ci.addedToGui = false;
            }
        } while ((evt = zcmIn->readNextEvent()) != nullptr);

        totalTimeUs = lastMsgUtime - firstMsgUtime;
        zcmIn->seekToTimestamp(firstMsgUtime);
        currMsgUtime = firstMsgUtime;

        if (!args.zcmUrlOut.empty()) {
            zcmOut = new zcm::ZCM(args.zcmUrlOut);
            if (!zcmOut->good()) {
                cerr << "Unable to initialize zcm" << endl;
                return false;
            }
        }

        return true;
    }

    bool init(int argc, char *argv[])
    {
        if (!args.init(argc, argv)) return false;
        loadPreferences();
        if (!initializeZcm()) return false;
        {
            unique_lock<mutex> lk(redrawLk);
            redrawNow = false;
        }
        thread redrawThr(&LogPlayer::redrawThrFunc, this);
        redrawThr.detach();
        return true;
    }

    void enableUI(bool enable)
    {
        gtk_widget_set_sensitive(btnPlay, enable);
        gtk_widget_set_sensitive(btnStep, enable);
        gtk_widget_set_sensitive(btnSlower, enable);
        gtk_widget_set_sensitive(btnFaster, enable);
        gtk_widget_set_sensitive(sclMacroScrub, enable);
        gtk_widget_set_sensitive(sclMicroScrub, enable);
        gtk_widget_set_sensitive(tblData, enable);
        gtk_widget_set_sensitive(btnToggle, enable);
        gtk_widget_set_sensitive(txtPrefix, enable);
    }

    void addChannels()
    {
        #pragma GCC diagnostic push
        #pragma GCC diagnostic ignored "-Wdeprecated-declarations"
        GtkTreeModel *model = gtk_tree_view_get_model(GTK_TREE_VIEW(tblData));
        for (auto& p : channelMap) {
            if (p.second.addedToGui) continue;
            const string& logChan = p.first;
            ChannelInfo& ci = p.second;
            GtkTreeIter iter;
            gtk_list_store_append(GTK_LIST_STORE(model), &iter);
            gtk_list_store_set(GTK_LIST_STORE(model), &iter,
                               LOG_CHAN_COLUMN, logChan.c_str(),
                               PLAY_CHAN_COLUMN, ci.pubChannel.c_str(),
                               ENABLED_COLUMN, ci.enabled, -1);
            ci.addedToGui = true;
        }
        #pragma GCC diagnostic pop
    }

    static gboolean zcmUpdateGui(LogPlayer *me)
    {
        unique_lock<mutex> lk(me->windowLk);
        if (!me->zcmIn || !me->zcmIn->good()) return G_SOURCE_CONTINUE;

        me->addChannels();

        double totTime = me->totalTimeUs / 1e6;
        double currTime = (me->currMsgUtime - me->firstMsgUtime) / 1e6;
        double currPerc = me->totalTimeUs == 0 ? 0 : currTime / totTime;

        ostringstream oss;
        oss << fixed << setprecision(1) << currTime << " s";
        gtk_label_set_text(GTK_LABEL(me->lblCurrTime), oss.str().c_str());

        oss.str("");
        oss << fixed << setprecision(1) << me->currSpeed << "x";
        gtk_label_set_text(GTK_LABEL(me->lblCurrSpeed), oss.str().c_str());

        if (!me->macroScrubIsDragging) {
            me->ignoreMacroScrubEvts++;
            gtk_range_set_value(GTK_RANGE(me->sclMacroScrub), currPerc);
        }

        if (!me->microScrubIsDragging) {
            double microScrubPerc;
            if (me->microScrubMax == me->microScrubMin) {
            microScrubPerc = 0.5;
            } else {
                double currOffset = (me->currMsgUtime - me->microPivotTimeUs) / 1e6;
                microScrubPerc = mathMap(currOffset, me->microScrubMin, me->microScrubMax, 0, 1);
                microScrubPerc = max(0.0, min(1.0, microScrubPerc));
            }
            me->ignoreMicroScrubEvts++;
            gtk_range_set_value(GTK_RANGE(me->sclMicroScrub), microScrubPerc);
        }

        bool isPlaying;
        {
            unique_lock<mutex> lk2(me->zcmLk);
            isPlaying = me->isPlaying;
        }

        if (isPlaying) {
            gtk_button_set_label(GTK_BUTTON(me->btnPlay), "Pause");
        } else {
            gtk_button_set_label(GTK_BUTTON(me->btnPlay), "Play");
        }

        if (me->args.exitWhenDone && currTime >= totTime) {
            done++;
            me->wakeup();
        }

        return G_SOURCE_CONTINUE;
    }

    void updateSpeedTarget()
    {
        ostringstream oss;
        oss << fixed << setprecision(1) << speedTarget << "x";
        gtk_label_set_text(GTK_LABEL(lblSpeedTarget), oss.str().c_str());
    }

    static void prefixChanged(GtkEntry *entry, LogPlayer *me)
    {
        const char *prefix = gtk_editable_get_text(GTK_EDITABLE(entry));
        #pragma GCC diagnostic push
        #pragma GCC diagnostic ignored "-Wdeprecated-declarations"
        GtkTreeModel *model = gtk_tree_view_get_model(GTK_TREE_VIEW(me->tblData));
        GtkTreeIter iter;

        if (gtk_tree_model_get_iter_first(model, &iter)) {
            do {
                char *logChan;
                gtk_tree_model_get(model, &iter, LOG_CHAN_COLUMN, &logChan, -1);
                string newPubChan = string(prefix) + string(logChan);
                gtk_list_store_set(GTK_LIST_STORE(model), &iter,
                                   PLAY_CHAN_COLUMN, newPubChan.c_str(), -1);
                me->channelMap[string(logChan)].pubChannel = newPubChan;
                g_free(logChan);
            } while (gtk_tree_model_iter_next(model, &iter));
        }
        #pragma GCC diagnostic pop
    }

    static void toggle(GtkButton *button, LogPlayer *me)
    {
        #pragma GCC diagnostic push
        #pragma GCC diagnostic ignored "-Wdeprecated-declarations"
        GtkTreeView *treeView = GTK_TREE_VIEW(me->tblData);
        GtkTreeSelection *selection = gtk_tree_view_get_selection(treeView);
        GtkTreeModel *model = gtk_tree_view_get_model(treeView);
        GList *selectedRows = gtk_tree_selection_get_selected_rows(selection, &model);

        for (GList *l = selectedRows; l != nullptr; l = l->next) {
            GtkTreePath *path = (GtkTreePath*)l->data;
            GtkTreeIter iter;
            gtk_tree_model_get_iter(model, &iter, path);
            char *logChan;
            gtk_tree_model_get(model, &iter, LOG_CHAN_COLUMN, &logChan, -1);
            me->toggleChannelPublish(string(logChan));
            g_free(logChan);
        }

        g_list_free_full(selectedRows, (GDestroyNotify)gtk_tree_path_free);
        #pragma GCC diagnostic pop
    }

    static void playbackChanEdit(GtkCellRendererText *renderer, char *path_str,
                                 char *new_text, LogPlayer *me)
    {
        #pragma GCC diagnostic push
        #pragma GCC diagnostic ignored "-Wdeprecated-declarations"
        GtkTreeModel *model = gtk_tree_view_get_model(GTK_TREE_VIEW(me->tblData));
        GtkTreePath *path = gtk_tree_path_new_from_string(path_str);
        GtkTreeIter iter;

        if (gtk_tree_model_get_iter(model, &iter, path)) {
            char *logChan;
            gtk_tree_model_get(model, &iter, LOG_CHAN_COLUMN, &logChan, -1);
            gtk_list_store_set(GTK_LIST_STORE(model), &iter,
                               PLAY_CHAN_COLUMN, new_text, -1);
            me->channelMap[string(logChan)].pubChannel = string(new_text);
            g_free(logChan);
        }
        gtk_tree_path_free(path);
        #pragma GCC diagnostic pop
    }

    static void channelEnable(GtkCellRendererToggle *renderer, char *path_str, LogPlayer *me)
    {
        #pragma GCC diagnostic push
        #pragma GCC diagnostic ignored "-Wdeprecated-declarations"
        GtkTreeModel *model = gtk_tree_view_get_model(GTK_TREE_VIEW(me->tblData));
        GtkTreePath *path = gtk_tree_path_new_from_string(path_str);
        GtkTreeIter iter;

        if (gtk_tree_model_get_iter(model, &iter, path)) {
            char *logChan;
            gtk_tree_model_get(model, &iter, LOG_CHAN_COLUMN, &logChan, -1);
            me->toggleChannelPublish(string(logChan));
            g_free(logChan);
        }
        gtk_tree_path_free(path);
        #pragma GCC diagnostic pop
    }

    void bookmark()
    {
        unique_lock<mutex> lk(zcmLk);
        double logTimePerc = totalTimeUs == 0 ? 0 : (currMsgUtime - firstMsgUtime) / (double) totalTimeUs;
        lk.unlock();
        addBookmark(BookmarkType::PLAIN, logTimePerc);
    }

    static void bookmarkClicked(GtkButton *button, LogPlayer *me)
    {
        me->bookmark();
    }

    void requestTime(uint64_t utime)
    {
        unique_lock<mutex> lk(zcmLk);
        requestTimeUs = utime;
        zcmCv.notify_all();
    }

    static gboolean onKeyPressed(GtkEventControllerKey *controller, guint keyval,
                                guint keycode, GdkModifierType state, LogPlayer *me)
    {
        GtkWidget *focusWidget = gtk_window_get_focus(GTK_WINDOW(me->window));
        if (focusWidget == me->txtPrefix || focusWidget == me->tblData)
            return FALSE;

        if (keyval == GDK_KEY_Escape) {
            gtk_window_set_focus(GTK_WINDOW(me->window), me->btnPlay);
            #pragma GCC diagnostic push
            #pragma GCC diagnostic ignored "-Wdeprecated-declarations"
            GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(me->tblData));
            gtk_tree_selection_unselect_all(selection);
            #pragma GCC diagnostic pop
        }

        switch (keyval) {
            case GDK_KEY_b:
                if (!me->bDown) {
                    me->bDown = true;
                    me->bookmark();
                }
                break;
            case GDK_KEY_KP_0:
            case GDK_KEY_0:
                if (me->bookmarks.size() <= 0) break;
                me->requestTime(me->firstMsgUtime +
                                me->bookmarks[0].logTimePerc * me->totalTimeUs);
                break;
            case GDK_KEY_KP_1:
            case GDK_KEY_1:
                if (me->bookmarks.size() <= 1) break;
                me->requestTime(me->firstMsgUtime +
                                me->bookmarks[1].logTimePerc * me->totalTimeUs);
                break;
            case GDK_KEY_KP_2:
            case GDK_KEY_2:
                if (me->bookmarks.size() <= 2) break;
                me->requestTime(me->firstMsgUtime +
                                me->bookmarks[2].logTimePerc * me->totalTimeUs);
                break;
            case GDK_KEY_KP_3:
            case GDK_KEY_3:
                if (me->bookmarks.size() <= 3) break;
                me->requestTime(me->firstMsgUtime +
                                me->bookmarks[3].logTimePerc * me->totalTimeUs);
                break;
            case GDK_KEY_KP_4:
            case GDK_KEY_4:
                if (me->bookmarks.size() <= 4) break;
                me->requestTime(me->firstMsgUtime +
                                me->bookmarks[4].logTimePerc * me->totalTimeUs);
                break;
            case GDK_KEY_KP_5:
            case GDK_KEY_5:
                if (me->bookmarks.size() <= 5) break;
                me->requestTime(me->firstMsgUtime +
                                me->bookmarks[5].logTimePerc * me->totalTimeUs);
                break;
            case GDK_KEY_KP_6:
            case GDK_KEY_6:
                if (me->bookmarks.size() <= 6) break;
                me->requestTime(me->firstMsgUtime +
                                me->bookmarks[6].logTimePerc * me->totalTimeUs);
                break;
            case GDK_KEY_KP_7:
            case GDK_KEY_7:
                if (me->bookmarks.size() <= 7) break;
                me->requestTime(me->firstMsgUtime +
                                me->bookmarks[7].logTimePerc * me->totalTimeUs);
                break;
            case GDK_KEY_KP_8:
            case GDK_KEY_8:
                if (me->bookmarks.size() <= 8) break;
                me->requestTime(me->firstMsgUtime +
                                me->bookmarks[8].logTimePerc * me->totalTimeUs);
                break;
            case GDK_KEY_KP_9:
            case GDK_KEY_9:
                if (me->bookmarks.size() <= 9) break;
                me->requestTime(me->firstMsgUtime +
                                me->bookmarks[9].logTimePerc * me->totalTimeUs);
                break;
        }
        return FALSE;
    }

    static gboolean onKeyReleased(GtkEventControllerKey *controller, guint keyval,
                                 guint keycode, GdkModifierType state, LogPlayer *me)
    {
        switch (keyval) {
            case GDK_KEY_b:
                me->bDown = false;
                break;
        }
        return FALSE;
    }

    static void macroScrub(GtkRange *range, LogPlayer *me)
    {
        if (me->ignoreMacroScrubEvts > 0) {
            me->ignoreMacroScrubEvts--;
            return;
        }
        gdouble pos = gtk_range_get_value(GTK_RANGE(range));
        me->requestTime(me->firstMsgUtime + pos * me->totalTimeUs);
        return;
    }

    static void onMacroScrubPressed(GtkGestureClick *gesture, int n_press,
                                   double x, double y, LogPlayer *me)
    {
        guint button = gtk_gesture_single_get_current_button(GTK_GESTURE_SINGLE(gesture));
        if (button == GDK_BUTTON_PRIMARY) {
            me->macroScrubIsDragging = true;
        } else if (button == GDK_BUTTON_SECONDARY) {
            me->bookmark();
        }
    }

    static void onMacroScrubReleased(GtkGestureClick *gesture, int n_press,
                                    double x, double y, LogPlayer *me)
    {
        guint button = gtk_gesture_single_get_current_button(GTK_GESTURE_SINGLE(gesture));
        if (button == GDK_BUTTON_PRIMARY) {
            me->macroScrubIsDragging = false;
        }
    }

    static void microScrub(GtkRange *range, LogPlayer *me)
    {
        if (me->ignoreMicroScrubEvts > 0) {
            me->ignoreMicroScrubEvts--;
            return;
        }
        gdouble pos = gtk_range_get_value(GTK_RANGE(range));
        pos = mathMap(pos, 0, 1, me->microScrubMin, me->microScrubMax);
        me->requestTime(me->microPivotTimeUs + pos * 1e6);
        return;
    }

    static void onMicroScrubPressed(GtkGestureClick *gesture, int n_press,
                                   double x, double y, LogPlayer *me)
    {
        guint button = gtk_gesture_single_get_current_button(GTK_GESTURE_SINGLE(gesture));
        if (button == GDK_BUTTON_PRIMARY) {
            me->microScrubIsDragging = true;
            {
                unique_lock<mutex> lk(me->zcmLk);
                me->microPivotTimeUs = me->currMsgUtime;
                me->microScrubWasPlayingOnStart = me->isPlaying;
                me->isPlaying = false;
            }
        } else if (button == GDK_BUTTON_SECONDARY) {
            me->bookmark();
        }
    }

    static void onMicroScrubReleased(GtkGestureClick *gesture, int n_press,
                                    double x, double y, LogPlayer *me)
    {
        guint button = gtk_gesture_single_get_current_button(GTK_GESTURE_SINGLE(gesture));
        if (button == GDK_BUTTON_PRIMARY) {
            me->microScrubIsDragging = false;
            double pos = mathMap(0, me->microScrubMin, me->microScrubMax, 0, 1);
            me->ignoreMicroScrubEvts++;
            gtk_range_set_value(GTK_RANGE(me->sclMicroScrub), pos);
            {
                unique_lock<mutex> lk(me->zcmLk);
                me->isPlaying = me->microScrubWasPlayingOnStart;
            }
            me->wakeup();
        }
    }

    static void fileDialogResponse(GtkDialog *dialog, int response, LogPlayer *me)
    {
        if (response == GTK_RESPONSE_ACCEPT) {
            #pragma GCC diagnostic push
            #pragma GCC diagnostic ignored "-Wdeprecated-declarations"
            GtkFileChooser *chooser = GTK_FILE_CHOOSER(dialog);
            GFile *file = gtk_file_chooser_get_file(chooser);
            #pragma GCC diagnostic pop
            char *filename = g_file_get_path(file);

            me->args.filename = string(filename);
            if (me->initializeZcm()) {
                gtk_label_set_text(GTK_LABEL(me->lblLogName), 
                                  StringUtil::basename(filename).c_str());
                me->enableUI(true);
                me->addChannels();
                for (size_t i = 0; i < me->bookmarks.size(); ++i) {
                    auto &b = me->bookmarks[i];
                    gtk_scale_add_mark(GTK_SCALE(me->sclMacroScrub),
                                       b.logTimePerc, GTK_POS_TOP,
                                       to_string(i).c_str());
                    b.addedToGui = true;
                }
                me->wakeup();
            } else {
                me->args.filename = "";
            }

            g_free(filename);
            g_object_unref(file);
        }
        gtk_window_destroy(GTK_WINDOW(dialog));
    }

    static void onLogNameDoubleClick(GtkGestureClick *gesture, int n_press,
                                    double x, double y, LogPlayer *me)
    {
        if (n_press == 2 && me->args.filename.empty()) {
            #pragma GCC diagnostic push
            #pragma GCC diagnostic ignored "-Wdeprecated-declarations"
            GtkWidget *dialog = gtk_file_chooser_dialog_new("Open Log File",
                                                            GTK_WINDOW(me->window),
                                                            GTK_FILE_CHOOSER_ACTION_OPEN,
                                                            "_Cancel", GTK_RESPONSE_CANCEL,
                                                            "_Open", GTK_RESPONSE_ACCEPT,
                                                            nullptr);
            #pragma GCC diagnostic pop

            g_signal_connect(dialog, "response", G_CALLBACK(fileDialogResponse), me);

            gtk_window_present(GTK_WINDOW(dialog));
        }
    }

    static void playPause(GtkButton *button, LogPlayer *me)
    {
        unique_lock<mutex> lk(me->zcmLk);
        me->isPlaying = !me->isPlaying;
        bool isPlaying = me->isPlaying;
        lk.unlock();
        
        if (isPlaying) {
            gtk_button_set_label(button, "Pause");
        } else {
            gtk_button_set_label(button, "Play");
        }
        me->wakeup();
    }

    static void step(GtkButton *button, LogPlayer *me)
    {
        unique_lock<mutex> lk(me->zcmLk);
        me->stepRequest = true;
        me->isPlaying = true;
        lk.unlock();
        
        gtk_button_set_label(GTK_BUTTON(me->btnPlay), "Pause");
        me->wakeup();
    }

    static void slow(GtkButton *button, LogPlayer *me)
    {
        unique_lock<mutex> lk(me->zcmLk);
        me->speedTarget /= 2;
        lk.unlock();
        me->updateSpeedTarget();
    }

    static void fast(GtkButton *button, LogPlayer *me)
    {
        unique_lock<mutex> lk(me->zcmLk);
        me->speedTarget *= 2;
        lk.unlock();
        me->updateSpeedTarget();
    }

    static gboolean windowCloseRequest(GtkWindow *window, LogPlayer *me)
    {
        unique_lock<mutex> lk(me->windowLk);
        me->window = nullptr;
        done++;
        me->wakeup();
        return FALSE;
    }

    static void windowDestroy(GtkWidget *widget, LogPlayer *me)
    {
        unique_lock<mutex> lk(me->windowLk);
        me->window = nullptr;
        done++;
        me->wakeup();
    }

    static void activate(GtkApplication *app, LogPlayer *me)
    {
        unique_lock<mutex> lk(me->windowLk);

        me->window = gtk_application_window_new(app);
        g_signal_connect(me->window, "close-request", G_CALLBACK(windowCloseRequest), me);
        g_signal_connect(me->window, "destroy", G_CALLBACK(windowDestroy), me);
        gtk_window_set_title(GTK_WINDOW(me->window), "ZCM Log Player");
        gtk_window_set_default_size(GTK_WINDOW(me->window), 450, 275);

        // Set up key event controller
        me->keyController = gtk_event_controller_key_new();
        gtk_widget_add_controller(me->window, me->keyController);
        g_signal_connect(me->keyController, "key-pressed", G_CALLBACK(onKeyPressed), me);
        g_signal_connect(me->keyController, "key-released", G_CALLBACK(onKeyReleased), me);

        GtkWidget *grid = gtk_grid_new();
        gtk_window_set_child(GTK_WINDOW(me->window), grid);
        gtk_grid_set_row_spacing(GTK_GRID(grid), 10);

        // Log name label with double-click gesture
        string logName = "Double click to load";
        if (!me->args.filename.empty())
            logName = StringUtil::basename(me->args.filename.c_str()).c_str();
        me->lblLogName = gtk_label_new(logName.c_str());
        gtk_widget_set_hexpand(me->lblLogName, TRUE);
        gtk_widget_set_halign(me->lblLogName, GTK_ALIGN_CENTER);
        
        if (me->args.filename.empty()) {
            me->logNameClickGesture = GTK_GESTURE_CLICK(gtk_gesture_click_new());
            gtk_widget_add_controller(me->lblLogName, GTK_EVENT_CONTROLLER(me->logNameClickGesture));
            g_signal_connect(me->logNameClickGesture, "pressed", G_CALLBACK(onLogNameDoubleClick), me);
        }
        gtk_grid_attach(GTK_GRID(grid), me->lblLogName, 0, 0, 1, 1);

        me->btnPlay = gtk_button_new_with_label("Play");
        g_signal_connect(me->btnPlay, "clicked", G_CALLBACK(playPause), me);
        gtk_widget_set_hexpand(me->btnPlay, TRUE);
        gtk_widget_set_halign(me->btnPlay, GTK_ALIGN_FILL);
        gtk_grid_attach(GTK_GRID(grid), me->btnPlay, 1, 0, 1, 1);

        me->btnStep = gtk_button_new_with_label("Step");
        g_signal_connect(me->btnStep, "clicked", G_CALLBACK(step), me);
        gtk_widget_set_hexpand(me->btnStep, TRUE);
        gtk_widget_set_halign(me->btnStep, GTK_ALIGN_FILL);
        gtk_grid_attach(GTK_GRID(grid), me->btnStep, 2, 0, 1, 1);

        me->btnSlower = gtk_button_new_with_label("<<");
        g_signal_connect(me->btnSlower, "clicked", G_CALLBACK(slow), me);
        gtk_widget_set_hexpand(me->btnSlower, TRUE);
        gtk_widget_set_halign(me->btnSlower, GTK_ALIGN_END);
        gtk_grid_attach(GTK_GRID(grid), me->btnSlower, 3, 0, 1, 1);

        me->lblSpeedTarget = gtk_label_new("");
        me->updateSpeedTarget();
        gtk_widget_set_hexpand(me->lblSpeedTarget, TRUE);
        gtk_widget_set_halign(me->lblSpeedTarget, GTK_ALIGN_CENTER);
        gtk_grid_attach(GTK_GRID(grid), me->lblSpeedTarget, 4, 0, 1, 1);

        me->btnFaster = gtk_button_new_with_label(">>");
        g_signal_connect(me->btnFaster, "clicked", G_CALLBACK(fast), me);
        gtk_widget_set_hexpand(me->btnFaster, TRUE);
        gtk_widget_set_halign(me->btnFaster, GTK_ALIGN_START);
        gtk_grid_attach(GTK_GRID(grid), me->btnFaster, 5, 0, 1, 1);

        // Right-click on scrubbers now directly creates bookmark
        me->menuScrub = nullptr;

        GtkAdjustment *adjMacroScrub = gtk_adjustment_new(0, 0, 1, 0.01, 0.05, 0);
        me->sclMacroScrub = gtk_scale_new(GTK_ORIENTATION_HORIZONTAL, adjMacroScrub);
        gtk_scale_set_draw_value(GTK_SCALE(me->sclMacroScrub), FALSE);
        gtk_widget_set_hexpand(me->sclMacroScrub, TRUE);
        gtk_widget_set_valign(me->sclMacroScrub, GTK_ALIGN_FILL);
        
        me->macroScrubClickGesture = GTK_GESTURE_CLICK(gtk_gesture_click_new());
        gtk_widget_add_controller(me->sclMacroScrub, GTK_EVENT_CONTROLLER(me->macroScrubClickGesture));
        g_signal_connect(me->macroScrubClickGesture, "pressed", G_CALLBACK(onMacroScrubPressed), me);
        g_signal_connect(me->macroScrubClickGesture, "released", G_CALLBACK(onMacroScrubReleased), me);
        g_signal_connect(me->sclMacroScrub, "value-changed", G_CALLBACK(macroScrub), me);
        gtk_grid_attach(GTK_GRID(grid), me->sclMacroScrub, 0, 1, 6, 1);

        GtkAdjustment *adjMicroScrub = gtk_adjustment_new(0, 0, 1, 0.01, 0.05, 0);
        me->sclMicroScrub = gtk_scale_new(GTK_ORIENTATION_HORIZONTAL, adjMicroScrub);
        gtk_scale_set_draw_value(GTK_SCALE(me->sclMicroScrub), FALSE);
        gtk_scale_set_has_origin(GTK_SCALE(me->sclMicroScrub), FALSE);
        gtk_widget_set_hexpand(me->sclMicroScrub, TRUE);
        gtk_widget_set_valign(me->sclMicroScrub, GTK_ALIGN_FILL);
        
        me->microScrubClickGesture = GTK_GESTURE_CLICK(gtk_gesture_click_new());
        gtk_widget_add_controller(me->sclMicroScrub, GTK_EVENT_CONTROLLER(me->microScrubClickGesture));
        g_signal_connect(me->microScrubClickGesture, "pressed", G_CALLBACK(onMicroScrubPressed), me);
        g_signal_connect(me->microScrubClickGesture, "released", G_CALLBACK(onMicroScrubReleased), me);
        g_signal_connect(me->sclMicroScrub, "value-changed", G_CALLBACK(microScrub), me);
        gtk_grid_attach(GTK_GRID(grid), me->sclMicroScrub, 0, 2, 6, 1);

        me->lblCurrTime = gtk_label_new("0 s");
        gtk_widget_set_hexpand(me->lblCurrTime, TRUE);
        gtk_widget_set_halign(me->lblCurrTime, GTK_ALIGN_START);
        gtk_grid_attach(GTK_GRID(grid), me->lblCurrTime, 0, 3, 2, 1);

        me->lblCurrSpeed = gtk_label_new("1.0x");
        gtk_widget_set_hexpand(me->lblCurrSpeed, TRUE);
        gtk_widget_set_halign(me->lblCurrSpeed, GTK_ALIGN_START);
        gtk_grid_attach(GTK_GRID(grid), me->lblCurrSpeed, 2, 3, 1, 1);

        #pragma GCC diagnostic push
        #pragma GCC diagnostic ignored "-Wdeprecated-declarations"
        me->tblData = gtk_tree_view_new();
        gtk_widget_set_vexpand(me->tblData, TRUE);
        gtk_widget_set_valign(me->tblData, GTK_ALIGN_FILL);
        gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(me->tblData), TRUE);

        GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(me->tblData));
        gtk_tree_selection_set_mode(selection, GTK_SELECTION_MULTIPLE);

        GtkListStore *store = gtk_list_store_new(NUM_COLUMNS,
                                                 G_TYPE_STRING, G_TYPE_STRING, G_TYPE_BOOLEAN);
        gtk_tree_view_set_model(GTK_TREE_VIEW(me->tblData), GTK_TREE_MODEL(store));
        gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(store), 0, GTK_SORT_ASCENDING);
        g_object_unref(store);

        GtkCellRenderer *logChanRenderer = gtk_cell_renderer_text_new();
        GtkTreeViewColumn *colLogChan =
            gtk_tree_view_column_new_with_attributes("Log Channel", logChanRenderer,
                                                     "text", LOG_CHAN_COLUMN, nullptr);
        gtk_tree_view_column_set_expand(colLogChan, TRUE);
        gtk_tree_view_column_set_resizable(colLogChan, TRUE);
        gtk_tree_view_append_column(GTK_TREE_VIEW(me->tblData), colLogChan);

        GtkCellRenderer *playbackChanRenderer = gtk_cell_renderer_text_new();
        g_object_set(playbackChanRenderer, "editable", TRUE, nullptr);
        g_signal_connect(playbackChanRenderer, "edited", G_CALLBACK(playbackChanEdit), me);
        GtkTreeViewColumn *colPlaybackChan =
            gtk_tree_view_column_new_with_attributes("Playback Channel",
                                                     playbackChanRenderer, "text",
                                                     PLAY_CHAN_COLUMN, nullptr);
        gtk_tree_view_column_set_resizable(colPlaybackChan, TRUE);
        gtk_tree_view_column_set_expand(colPlaybackChan, TRUE);
        gtk_tree_view_append_column(GTK_TREE_VIEW(me->tblData), colPlaybackChan);

        GtkCellRenderer *enableRenderer = gtk_cell_renderer_toggle_new();
        GtkTreeViewColumn *colEnable =
            gtk_tree_view_column_new_with_attributes("Enable", enableRenderer,
                                                     "active", ENABLED_COLUMN, nullptr);
        gtk_tree_view_column_set_resizable(colEnable, TRUE);
        gtk_tree_view_column_set_expand(colEnable, TRUE);
        g_signal_connect(enableRenderer, "toggled", G_CALLBACK(channelEnable), me);
        gtk_tree_view_append_column(GTK_TREE_VIEW(me->tblData), colEnable);
        #pragma GCC diagnostic pop

        GtkWidget *scrolled_window = gtk_scrolled_window_new();
        gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrolled_window), me->tblData);
        gtk_grid_attach(GTK_GRID(grid), scrolled_window, 0, 4, 6, 1);

        me->btnToggle = gtk_button_new_with_label("Toggle Selected");
        g_signal_connect(me->btnToggle, "clicked", G_CALLBACK(toggle), me);
        gtk_widget_set_hexpand(me->btnToggle, TRUE);
        gtk_widget_set_halign(me->btnToggle, GTK_ALIGN_FILL);
        gtk_grid_attach(GTK_GRID(grid), me->btnToggle, 0, 5, 1, 1);

        GtkWidget *lblPrefix = gtk_label_new("Channel Prefix:");
        gtk_widget_set_hexpand(lblPrefix, TRUE);
        gtk_widget_set_halign(lblPrefix, GTK_ALIGN_CENTER);
        gtk_grid_attach(GTK_GRID(grid), lblPrefix, 1, 5, 1, 1);

        me->txtPrefix = gtk_entry_new();
        gtk_widget_set_hexpand(me->txtPrefix, TRUE);
        gtk_widget_set_halign(me->txtPrefix, GTK_ALIGN_CENTER);
        g_signal_connect(me->txtPrefix, "changed", G_CALLBACK(prefixChanged), me);
        gtk_grid_attach(GTK_GRID(grid), me->txtPrefix, 2, 5, 4, 1);

        if (me->zcmIn && me->zcmIn->good()) {
            me->enableUI(true);
            me->addChannels();
            for (size_t i = 0; i < me->bookmarks.size(); ++i) {
                auto &b = me->bookmarks[i];
                gtk_scale_add_mark(GTK_SCALE(me->sclMacroScrub),
                                   b.logTimePerc, GTK_POS_TOP,
                                   to_string(i).c_str());
                b.addedToGui = true;
            }
        } else {
            me->enableUI(false);
        }

        gtk_window_present(GTK_WINDOW(me->window));
        
        // Start GUI update timer
        g_timeout_add(50, (GSourceFunc)zcmUpdateGui, me);
    }

    void quit()
    {
        unique_lock<mutex> lk(windowLk);
        if (window) gtk_window_close(GTK_WINDOW(window));
    }

    void redraw()
    {
        unique_lock<mutex> lk(redrawLk);
        redrawNow = true;
        redrawCv.notify_all();
    }

    void redrawThrFunc()
    {
        while (true) {
            unique_lock<mutex> lk(redrawLk);
            redrawCv.wait(lk, [&](){ return done || redrawNow; });
            if (done) return;
            redrawNow = false;
            lk.unlock();
            
            this_thread::sleep_for(chrono::milliseconds(50));
        }
    }

    void playThrFunc()
    {
        // Wait for GUI to be ready
        this_thread::sleep_for(chrono::milliseconds(100));
        
        if (!zcmIn || !zcmIn->good()) {
            cerr << "No valid log file loaded" << endl;
            return;
        }

        zcmIn->seekToTimestamp(firstMsgUtime);
        
        auto lastDispatchTime = chrono::steady_clock::now();
        uint64_t lastLogUtime = firstMsgUtime;
        
        while (!done) {
            bool _isPlaying;
            double _speedTarget;
            uint64_t _requestTimeUs;
            bool _stepRequest;
            
            {
                unique_lock<mutex> lk(zcmLk);
                zcmCv.wait(lk, [&](){
                    return done || isPlaying || 
                           requestTimeUs != numeric_limits<uint64_t>::max() ||
                           stepRequest;
                });
                
                if (done) break;
                
                _isPlaying = isPlaying;
                _speedTarget = speedTarget;
                _requestTimeUs = requestTimeUs;
                _stepRequest = stepRequest;
                
                requestTimeUs = numeric_limits<uint64_t>::max();
                if (_stepRequest) {
                    stepRequest = false;
                    isPlaying = false;
                }
            }
            
            if (_requestTimeUs != numeric_limits<uint64_t>::max()) {
                zcmIn->seekToTimestamp(_requestTimeUs);
                lastDispatchTime = chrono::steady_clock::now();
                lastLogUtime = _requestTimeUs;
            }
            
            const zcm::LogEvent *le = zcmIn->readNextEvent();
            if (!le) {
                unique_lock<mutex> lk(zcmLk);
                isPlaying = false;
                currMsgUtime = firstMsgUtime + totalTimeUs;
                continue;
            }
            
            {
                unique_lock<mutex> lk(zcmLk);
                currMsgUtime = le->timestamp;
            }
            
            if (!_isPlaying && !_stepRequest) {
                this_thread::sleep_for(chrono::milliseconds(10));
                continue;
            }
            
            // Calculate timing
            auto now = chrono::steady_clock::now();
            auto realElapsed = chrono::duration_cast<chrono::microseconds>(now - lastDispatchTime);
            uint64_t logElapsed = le->timestamp - lastLogUtime;
            uint64_t targetElapsed = logElapsed / _speedTarget;
            
            if ((uint64_t)realElapsed.count() < targetElapsed) {
                auto sleepTime = chrono::microseconds(targetElapsed - realElapsed.count());
                this_thread::sleep_for(sleepTime);
            }
            
            // Publish message
            auto itr = channelMap.find(le->channel);
            if (itr != channelMap.end() && itr->second.enabled && zcmOut) {
                zcmOut->publish(itr->second.pubChannel.c_str(), le->data, le->datalen);
            }
            
            // Calculate actual speed
            now = chrono::steady_clock::now();
            realElapsed = chrono::duration_cast<chrono::microseconds>(now - lastDispatchTime);
            if (realElapsed.count() > 0) {
                currSpeed = (double)logElapsed / realElapsed.count() * _speedTarget;
            }
            
            lastDispatchTime = now;
            lastLogUtime = le->timestamp;
            
            if (_stepRequest) {
                unique_lock<mutex> lk(zcmLk);
                isPlaying = false;
            }
        }
    }

    int run()
    {
        GtkApplication *app = gtk_application_new("org.zcm.logplayer", G_APPLICATION_NON_UNIQUE);

        thread playThr(&LogPlayer::playThrFunc, this);

        g_signal_connect(app, "activate", G_CALLBACK(activate), this);
        int ret = g_application_run(G_APPLICATION(app), 0, nullptr);

        done = 1;
        wakeup();
        {
            unique_lock<mutex> lk(zcmLk);
            zcmCv.notify_all();
        }

        g_object_unref(app);

        playThr.join();
        savePreferences();

        return ret;
    }
};

static LogPlayer lp;

static void sighandler(int signal)
{
    done++;
    if (done == 3) exit(1);
    lp.wakeup();
}

int main(int argc, char *argv[])
{
    if (!lp.init(argc, argv)) return 1;

    // Register signal handlers
    signal(SIGINT, sighandler);
    signal(SIGQUIT, sighandler);
    signal(SIGTERM, sighandler);

    return lp.run();
}