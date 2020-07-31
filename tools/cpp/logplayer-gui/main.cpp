#include <atomic>
#include <getopt.h>
#include <gtk/gtk.h>
#include <iostream>
#include <limits>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <unordered_map>

#include <zcm/zcm-cpp.hpp>

#include "util/TimeUtil.hpp"

using namespace std;

static atomic_int done {0};

struct Args
{
    string filename = "";
    string zcmUrlOut = "";
    bool highAccuracyMode = false;
    bool verbose = false;

    bool init(int argc, char *argv[])
    {
        struct option long_opts[] = {
            { "help",                no_argument, 0, 'h' },
            { "zcm-url",       required_argument, 0, 'u' },
            { "high-accuracy",       no_argument, 0, 'a' },
            { "verbose",             no_argument, 0, 'v' },
            { 0, 0, 0, 0 }
        };

        int c;
        while ((c = getopt_long(argc, argv, "hu:av", long_opts, 0)) >= 0) {
            switch (c) {
                case 'u':        zcmUrlOut = string(optarg);       break;
                case 'a': highAccuracyMode = true;                 break;
                case 'v':          verbose = true;                 break;
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
    GtkWidget *btnPlay;
    GtkWidget *btnStep;
    GtkWidget *btnSlower;
    GtkWidget *lblSpeed;
    GtkWidget *btnFaster;
    GtkWidget *lblCurrTime;
    GtkWidget *sclMacroScrub;
    GtkWidget *sclMicroScrub;
    GtkWidget *tblData;
    GtkWidget *btnToggle;

    uint64_t totalTimeUs;
    uint64_t firstMsgUtime;

    mutex zcmLk;
    condition_variable cv;
    int isPlaying = 0;
    double speedTarget = 1.0;
    bool ignoreMicroScrubEvts = false;
    bool ignoreMacroScrubEvts = false;
    uint64_t currMsgUtime = 0;
    uint64_t requestTimeUs = numeric_limits<uint64_t>::max();
    bool stepRequest = false;
    string stepPrefix;

    struct ChannelInfo
    {
        string pubChannel;
        bool enabled;
        bool addedToGui;
    };
    unordered_map<string, ChannelInfo> channelMap;

    LogPlayer() {}

    ~LogPlayer()
    {
        if (zcmIn)  { delete zcmIn;  }
        if (zcmOut) { delete zcmOut; }
    }

    void wakeup() { cv.notify_all(); }

    bool toggleChannelPublish(GtkTreeIter iter)
    {
        GtkTreeModel *model = gtk_tree_view_get_model(GTK_TREE_VIEW(tblData));
        gchar *name;
        gtk_tree_model_get(model, &iter, LOG_CHAN_COLUMN, &name, -1);

        bool enabled;
        {
            unique_lock<mutex> lk(zcmLk);
            channelMap[name].enabled = !channelMap[name].enabled;
            enabled = channelMap[name].enabled;
        }

        gtk_list_store_set(GTK_LIST_STORE(model), &iter, ENABLED_COLUMN, enabled, -1);

        return enabled;
    }

    bool initializeZcm()
    {
        zcmOut = new zcm::ZCM(args.zcmUrlOut);
        if (!zcmOut->good()) {
            cerr << "Error: Failed to open zcm: '" << args.zcmUrlOut << "'" << endl;
            return false;
        }

        if (args.filename.empty()) return false;

        zcmIn = new zcm::LogFile(args.filename, "r");
        if (!zcmIn->good()) {
            cerr << "Error: Failed to open '" << args.filename << "'" << endl;
            return false;
        }

        const zcm::LogEvent* leStart = zcmIn->readNextEvent();
        firstMsgUtime = (uint64_t)leStart->timestamp;

        zcmIn->seekToTimestamp(numeric_limits<int64_t>::max());

        const zcm::LogEvent* leEnd = zcmIn->readPrevEvent();
        if (!leEnd) leEnd = zcmIn->readPrevEvent(); // In case log was cut off at end
        assert(leEnd);

        uint64_t lastMsgUtime = (uint64_t)leEnd->timestamp;

        assert(lastMsgUtime > firstMsgUtime);
        totalTimeUs = lastMsgUtime - firstMsgUtime;

        return true;
    }

    bool init(int argc, char *argv[])
    {
        if (!args.init(argc, argv))
            return false;

        initializeZcm();

        return true;
    }

    void enableUI(gboolean enable)
    {
        gtk_widget_set_sensitive(btnPlay, enable);
        gtk_widget_set_sensitive(btnStep, enable);
        gtk_widget_set_sensitive(btnSlower, enable);
        gtk_widget_set_sensitive(btnFaster, enable);
        gtk_widget_set_sensitive(sclMacroScrub, enable);
        gtk_widget_set_sensitive(sclMicroScrub, enable);
        gtk_widget_set_sensitive(tblData, enable);
        gtk_widget_set_sensitive(btnToggle, enable);
    }

    void addChannels(unordered_map<string, ChannelInfo> channelMap)
    {
        GtkTreeModel *model = gtk_tree_view_get_model(GTK_TREE_VIEW(tblData));

        for (const auto& c : channelMap) {
            if (c.second.addedToGui) continue;
            GtkTreeIter iter;
            gtk_list_store_append(GTK_LIST_STORE(model), &iter);
            gtk_list_store_set(GTK_LIST_STORE(model), &iter,
                               LOG_CHAN_COLUMN, c.first.c_str(), -1);
            gtk_list_store_set(GTK_LIST_STORE(model), &iter,
                               PLAY_CHAN_COLUMN, c.second.pubChannel.c_str(), -1);
            gtk_list_store_set(GTK_LIST_STORE(model), &iter,
                               ENABLED_COLUMN, c.second.enabled, -1);
        }
    }

    static gboolean zcmUpdateGui(LogPlayer *me)
    {
        unordered_map<string, ChannelInfo> _channelMap;
        double currTimeS;
        double totalTimeS;
        float perc;
        {
            unique_lock<mutex> lk(me->zcmLk);
            perc = (float) (me->currMsgUtime - me->firstMsgUtime) / (float) me->totalTimeUs;
            _channelMap = me->channelMap;
            for (auto& c : me->channelMap) c.second.addedToGui = true;
            currTimeS = (me->currMsgUtime - me->firstMsgUtime) / 1e6;
            totalTimeS = me->totalTimeUs / 1e6;
        }
        {
            unique_lock<mutex> lk(me->windowLk);

            if (!me->window) return FALSE;

            me->ignoreMacroScrubEvts = true;
            gtk_range_set_value(GTK_RANGE(me->sclMacroScrub), perc);

            gchar currTimeStr[50];
            g_snprintf(currTimeStr, 50, "%.2f s / %.2f s", currTimeS, totalTimeS);
            gtk_label_set_text(GTK_LABEL(me->lblCurrTime), currTimeStr);

            me->addChannels(_channelMap);
        }

        return FALSE;
    }

    void updateSpeed()
    {
        gchar speedStr[20];
        g_snprintf(speedStr, 20, "%.3f", speedTarget);
        gtk_label_set_text(GTK_LABEL(lblSpeed), speedStr);
    }

    static void prefixChanged(GtkEditable *editable, LogPlayer *me)
    {
        const gchar *prefix;
        prefix = gtk_entry_get_text(GTK_ENTRY(editable));
        {
            unique_lock<mutex> lk(me->zcmLk);
            me->stepPrefix = prefix;
            me->stepRequest = false;
            me->isPlaying = false;
        }
    }

    static void toggle(GtkWidget *widget, LogPlayer *me)
    {
        GtkTreeSelection *selection;
        GtkTreeModel     *model;
        GtkTreeIter       iter;

        selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(me->tblData));

        if (gtk_tree_selection_get_selected(selection, &model, &iter))
            me->toggleChannelPublish(iter);
    }

    static void playbackChanEdit(GtkCellRendererText *cell,
                                 gchar *path, gchar *newChan,
                                 GtkListStore *model)
    {
        GtkTreeIter iter;
        gchar *oldChan;

        gtk_tree_model_get_iter_from_string(GTK_TREE_MODEL(model), &iter, path);

        gtk_tree_model_get(GTK_TREE_MODEL(model), &iter, PLAY_CHAN_COLUMN, &oldChan, -1);
        gtk_list_store_set(GTK_LIST_STORE(model), &iter, PLAY_CHAN_COLUMN, newChan, -1);
    }

    static void channelEnable(GtkCellRendererToggle *cell, gchar *path, LogPlayer *me)
    {
        GtkTreeModel *model = gtk_tree_view_get_model(GTK_TREE_VIEW(me->tblData));

        GtkTreeIter iter;
        gtk_tree_model_get_iter_from_string(GTK_TREE_MODEL(model), &iter, path);

        me->toggleChannelPublish(iter);
    }

    static void macroScrub(GtkRange *range, LogPlayer *me)
    {
        if (me->ignoreMacroScrubEvts) {
            me->ignoreMacroScrubEvts = false;
            return;
        }
        gdouble pos = gtk_range_get_value(range);
        {
            unique_lock<mutex> lk(me->zcmLk);
            me->requestTimeUs = me->firstMsgUtime + pos * me->totalTimeUs;
        }
    }

    static void microScrub(GtkRange *range, LogPlayer *me)
    {
        if (me->ignoreMicroScrubEvts) {
            me->ignoreMicroScrubEvts = false;
            return;
        }
        //gdouble pos = gtk_range_get_value(range);
        me->ignoreMicroScrubEvts = true;
        gtk_range_set_value(range, 0);
    }

    void selectLog()
    {
        assert(args.filename == "");

        GtkWidget *dialog = gtk_file_chooser_dialog_new("Open File",
                                                        GTK_WINDOW(window),
                                                        GTK_FILE_CHOOSER_ACTION_OPEN,
                                                        "Cancel",
                                                        GTK_RESPONSE_CANCEL,
                                                        "Open",
                                                        GTK_RESPONSE_ACCEPT,
                                                        NULL);
        gint res = gtk_dialog_run(GTK_DIALOG(dialog));

        if (res == GTK_RESPONSE_ACCEPT) {
            char *filename;
            GtkFileChooser *chooser = GTK_FILE_CHOOSER(dialog);
            filename = gtk_file_chooser_get_filename(chooser);
            args.filename = string(filename);
            g_print("Opening log: %s\n", args.filename.c_str());
            g_free(filename);
        }

        gtk_widget_destroy(dialog);
    }

    static gboolean openLog(GtkWidget *widget, GdkEventButton *event, LogPlayer* me)
    {
        if (event->type == GDK_2BUTTON_PRESS)
            me->selectLog();
        return TRUE;
    }

    static void playPause(GtkWidget *widget, LogPlayer *me)
    {
        bool isPlaying;
        {
            unique_lock<mutex> lk(me->zcmLk);
            me->isPlaying = !me->isPlaying;
            isPlaying = me->isPlaying;
            me->wakeup();
        }
        if (isPlaying) {
            gtk_button_set_label(GTK_BUTTON(widget), "Pause");
        } else {
            gtk_button_set_label(GTK_BUTTON(widget), "Play");
        }
    }

    static void step(GtkWidget *widget, LogPlayer *me)
    {
        bool isPlaying;;
        {
            unique_lock<mutex> lk(me->zcmLk);
            if (!me->stepPrefix.empty()) {
                me->stepRequest = true;
                me->isPlaying = true;
            }
            isPlaying = me->isPlaying;
        }
        if (isPlaying) {
            gtk_button_set_label(GTK_BUTTON(me->btnPlay), "Pause");
        } else {
            gtk_button_set_label(GTK_BUTTON(me->btnPlay), "Play");
        }
    }

    static void slow(GtkWidget *widget, LogPlayer *me)
    {
        unique_lock<mutex> lk(me->zcmLk);
        me->speedTarget /= 2;
        me->updateSpeed();
    }

    static void fast(GtkWidget *widget, LogPlayer *me)
    {
        unique_lock<mutex> lk(me->zcmLk);
        me->speedTarget *= 2;
        me->updateSpeed();
    }

    static gboolean windowDelete(GtkWidget *widget, GdkEvent *event, LogPlayer *me)
    {
        unique_lock<mutex> lk(me->windowLk);
        me->window = NULL;
        return FALSE;
    }

    static void windowDestroy(GtkWidget *widget, LogPlayer *me)
    {
        unique_lock<mutex> lk(me->windowLk);
        me->window = NULL;
    }

    static void activate(GtkApplication *app, LogPlayer *me)
    {
        unique_lock<mutex> lk(me->windowLk);

        me->window = gtk_application_window_new(app);
        g_signal_connect(me->window, "delete-event", G_CALLBACK(windowDelete), me);
        g_signal_connect(me->window, "destroy", G_CALLBACK(windowDestroy), me);
        gtk_window_set_title(GTK_WINDOW(me->window), "Zcm Log Player");
        gtk_window_set_default_size(GTK_WINDOW(me->window), 475, 275);
        gtk_container_set_border_width(GTK_CONTAINER(me->window), 1);
        gtk_widget_add_events(me->window, GDK_BUTTON_PRESS_MASK);

        GtkWidget *grid = gtk_grid_new();
        gtk_container_add(GTK_CONTAINER(me->window), grid);
        gtk_grid_set_row_spacing(GTK_GRID(grid), 10);

        GtkWidget *evtLogName = gtk_event_box_new();
        gtk_event_box_set_above_child(GTK_EVENT_BOX(evtLogName), TRUE);
        gtk_grid_attach(GTK_GRID(grid), evtLogName, 0, 0, 1, 1);
        gtk_widget_set_events(evtLogName, GDK_BUTTON_PRESS_MASK);

        GtkWidget *lblLogName = gtk_label_new("Double click to load");
        gtk_widget_set_hexpand(lblLogName, TRUE);
        gtk_widget_set_halign(lblLogName, GTK_ALIGN_CENTER);
        g_signal_connect(G_OBJECT(evtLogName), "button_press_event",
                         G_CALLBACK(openLog), me);
        gtk_container_add(GTK_CONTAINER(evtLogName), lblLogName);


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

        me->lblSpeed = gtk_label_new("");
        me->updateSpeed();
        gtk_widget_set_hexpand(me->lblSpeed, TRUE);
        gtk_widget_set_halign(me->lblSpeed, GTK_ALIGN_CENTER);
        gtk_grid_attach(GTK_GRID(grid), me->lblSpeed, 4, 0, 1, 1);

        me->btnFaster = gtk_button_new_with_label(">>");
        g_signal_connect(me->btnFaster, "clicked", G_CALLBACK(fast), me);
        gtk_widget_set_hexpand(me->btnFaster, TRUE);
        gtk_widget_set_halign(me->btnFaster, GTK_ALIGN_START);
        gtk_grid_attach(GTK_GRID(grid), me->btnFaster, 5, 0, 1, 1);

        GtkAdjustment *adjMacroScrub = gtk_adjustment_new(0, 0, 1, 0.01, 0.1, 0);
        me->sclMacroScrub = gtk_scale_new(GTK_ORIENTATION_HORIZONTAL, adjMacroScrub);
        gtk_scale_set_draw_value(GTK_SCALE(me->sclMacroScrub), FALSE);
        gtk_widget_set_hexpand(me->sclMacroScrub, TRUE);
        gtk_widget_set_valign(me->sclMacroScrub, GTK_ALIGN_FILL);
        g_signal_connect(me->sclMacroScrub, "value-changed", G_CALLBACK(macroScrub), me);
        gtk_grid_attach(GTK_GRID(grid), me->sclMacroScrub, 0, 1, 6, 1);

        GtkAdjustment *adjMicroScrub = gtk_adjustment_new(0, -1, 1, 0.01, 0.1, 0);
        me->sclMicroScrub = gtk_scale_new(GTK_ORIENTATION_HORIZONTAL, adjMicroScrub);
        gtk_scale_set_draw_value(GTK_SCALE(me->sclMicroScrub), FALSE);
        gtk_scale_set_has_origin(GTK_SCALE(me->sclMicroScrub), FALSE);
        gtk_widget_set_hexpand(me->sclMicroScrub, TRUE);
        gtk_widget_set_valign(me->sclMicroScrub, GTK_ALIGN_FILL);
        g_signal_connect(me->sclMicroScrub, "value-changed", G_CALLBACK(microScrub), me);
        gtk_grid_attach(GTK_GRID(grid), me->sclMicroScrub, 0, 2, 6, 1);

        me->lblCurrTime = gtk_label_new("0 s");
        gtk_widget_set_hexpand(me->lblCurrTime, TRUE);
        gtk_widget_set_halign(me->lblCurrTime, GTK_ALIGN_CENTER);
        gtk_grid_attach(GTK_GRID(grid), me->lblCurrTime, 0, 3, 1, 1);

        me->tblData = gtk_tree_view_new();
        gtk_widget_set_vexpand(me->tblData, TRUE);
        gtk_widget_set_valign(me->tblData, GTK_ALIGN_FILL);
        gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(me->tblData), TRUE);

        GtkListStore *store = gtk_list_store_new(NUM_COLUMNS,
                                                 G_TYPE_STRING, G_TYPE_STRING, G_TYPE_BOOLEAN);
        gtk_tree_view_set_model(GTK_TREE_VIEW(me->tblData), GTK_TREE_MODEL(store));
        g_object_unref(store);

        GtkCellRenderer *logChanRenderer = gtk_cell_renderer_text_new();
        GtkTreeViewColumn *colLogChan =
            gtk_tree_view_column_new_with_attributes("Log Channel", logChanRenderer,
                                                     "text", LOG_CHAN_COLUMN, NULL);
        gtk_tree_view_column_set_resizable(colLogChan, TRUE);
        gtk_tree_view_append_column(GTK_TREE_VIEW(me->tblData), colLogChan);

        GtkCellRenderer *playbackChanRenderer = gtk_cell_renderer_text_new();
        g_object_set(playbackChanRenderer, "editable", TRUE, NULL);
        g_signal_connect(playbackChanRenderer, "edited", G_CALLBACK(playbackChanEdit), me);
        GtkTreeViewColumn *colPlaybackChan =
            gtk_tree_view_column_new_with_attributes("Playback Channel",
                                                     playbackChanRenderer, "text",
                                                     PLAY_CHAN_COLUMN, NULL);
        gtk_tree_view_column_set_resizable(colPlaybackChan, TRUE);
        gtk_tree_view_append_column(GTK_TREE_VIEW(me->tblData), colPlaybackChan);

        GtkCellRenderer *enableRenderer = gtk_cell_renderer_toggle_new();
        GtkTreeViewColumn *colEnable =
            gtk_tree_view_column_new_with_attributes("Enable", enableRenderer,
                                                     "active", ENABLED_COLUMN, NULL);
        gtk_tree_view_column_set_resizable(colEnable, TRUE);
        g_signal_connect(enableRenderer, "toggled", G_CALLBACK(channelEnable), me);
        gtk_tree_view_append_column(GTK_TREE_VIEW(me->tblData), colEnable);

        //GtkWidget *scrolled_window = gtk_scrolled_window_new(NULL, NULL);
        //gtk_container_add(GTK_CONTAINER(scrolled_window), me->tblData);
        gtk_grid_attach(GTK_GRID(grid), me->tblData, 0, 4, 6, 1);

        me->btnToggle = gtk_button_new_with_label("Toggle Selected");
        g_signal_connect(me->btnToggle, "clicked", G_CALLBACK(toggle), me);
        gtk_widget_set_hexpand(me->btnToggle, TRUE);
        gtk_widget_set_halign(me->btnToggle, GTK_ALIGN_FILL);
        gtk_grid_attach(GTK_GRID(grid), me->btnToggle, 0, 5, 1, 1);

        GtkWidget *lblPrefix = gtk_label_new("Channel Prefix:");
        gtk_widget_set_hexpand(lblPrefix, TRUE);
        gtk_widget_set_halign(lblPrefix, GTK_ALIGN_CENTER);
        gtk_grid_attach(GTK_GRID(grid), lblPrefix, 1, 5, 1, 1);

        GtkWidget *txtPrefix = gtk_entry_new();
        gtk_widget_set_hexpand(txtPrefix, TRUE);
        gtk_widget_set_halign(txtPrefix, GTK_ALIGN_CENTER);
        g_signal_connect(txtPrefix, "changed", G_CALLBACK(prefixChanged), me);
        gtk_grid_attach(GTK_GRID(grid), txtPrefix, 2, 5, 4, 1);

        me->enableUI(me->zcmIn && me->zcmIn->good());
        //*
        // add_sample_data(store);
        // add_sample_data(store);
        // add_sample_data(store);
        // */

        gtk_widget_show_all(me->window);
    }

    void playThrFunc()
    {
        zcmIn->seekToTimestamp(0);

        uint64_t lastDispatchUtime = numeric_limits<uint64_t>::max();;;
        uint64_t lastLogUtime = numeric_limits<uint64_t>::max();;
        float lastSpeedTarget = numeric_limits<float>::max();

        const zcm::LogEvent *le;

        while (true) {

            float _speedTarget;
            bool wasPaused = false;
            uint64_t _requestTimeUs;

            {
                unique_lock<mutex> lk(zcmLk);
                cv.wait_for(lk, chrono::milliseconds(500), [&](){
                    bool wakeup = done || isPlaying;
                    wasPaused |= !wakeup;
                    return wakeup;
                });
                if (done) break;
                if (!isPlaying) continue;

                _speedTarget = speedTarget;

                _requestTimeUs = requestTimeUs;
                requestTimeUs = numeric_limits<uint64_t>::max();
            }

            bool reset = _speedTarget != lastSpeedTarget || wasPaused;

            if (_requestTimeUs != numeric_limits<uint64_t>::max()) {
                zcmIn->seekToTimestamp(_requestTimeUs);
                reset = true;
            }

            le = zcmIn->readNextEvent();
            if (!le) break;

            uint64_t nowUs = TimeUtil::utime();

            if (reset) {
                lastLogUtime = le->timestamp;
                lastDispatchUtime = nowUs;
                lastSpeedTarget = _speedTarget;
            }

            // Total time difference from now to publishing the first message
            // is zero in first run
            uint64_t localDiffUs = nowUs - lastDispatchUtime;
            // Total difference of timestamps of the current and first message
            uint64_t logDiffUs = (uint64_t) le->timestamp - lastLogUtime;
            uint64_t logDiffSpeedUs = logDiffUs / _speedTarget;
            uint64_t diffUs = logDiffSpeedUs > localDiffUs ? logDiffSpeedUs - localDiffUs : 0;
            // Ensure nanosleep wakes up before the range of uncertainty of
            // the OS scheduler would impact our sleep time. Then we busy wait
            const uint64_t busyWaitUs = args.highAccuracyMode ? 10000 : 0;
            diffUs = diffUs > busyWaitUs ? diffUs - busyWaitUs : 0;
            // Introducing time differences to starting times rather than last loop
            // times eliminates linear increase of delay when message are published
            timespec delay;
            delay.tv_sec = (long int) diffUs / 1000000;
            delay.tv_nsec = (long int) (diffUs - (delay.tv_sec * 1000000)) * 1000;

            // Sleep until we're supposed to wake up and busy wait
            if (diffUs > 0) nanosleep(&delay, nullptr);
            // Busy wait the rest
            while (logDiffSpeedUs > TimeUtil::utime() - lastDispatchUtime);

            if (args.verbose)
                printf("%.3f Channel %-20s size %d\n", le->timestamp / 1e6,
                       le->channel.c_str(), le->datalen);

            ChannelInfo c;

            if (!channelMap.count(le->channel)) {
                c.pubChannel = le->channel;
                c.enabled = true;
                c.addedToGui = false;
                channelMap[le->channel] = c;
            }

            c = channelMap[le->channel];

            if (c.enabled) zcmOut->publish(c.pubChannel.c_str(), le->data, le->datalen);

            {
                unique_lock<mutex> lk(zcmLk);
                currMsgUtime = (uint64_t) le->timestamp;
                if (stepRequest && le->channel.rfind(stepPrefix, 0) == 0)
                    isPlaying = false;
            }

            g_idle_add((GSourceFunc)zcmUpdateGui, this);
        }

        {
            unique_lock<mutex> lk(windowLk);
            cout << "Quitting player thread" << endl;
            if (window) gtk_window_close(GTK_WINDOW(window));
        }
    }

    int run()
    {
        GtkApplication *app = gtk_application_new("org.zcm.logplayer", G_APPLICATION_FLAGS_NONE);

        thread thr(&LogPlayer::playThrFunc, this);

        g_signal_connect(app, "activate", G_CALLBACK(activate), this);
        int ret = g_application_run(G_APPLICATION(app), 0, NULL);

        done = 1;

        g_object_unref(app);

        thr.join();

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
