# ZCM Tools

ZCM ships with a convenient set of debugging and monitoring tools. In this section
we demonstrate the usage of these tools.

### Spy
##### To mark for build: `$./waf configure --use-java`

Since ZCM makes data decoupling so easy, developers tend to build applications in
several modules/processes and tie them together with ZCM message passing. In this
model, a lot of the debugging takes place at the message-passing level. Often
it's desirable to inspect/spy the messages in transit. This can be accomplished
using the `zcm-spy` tool. Note that you must have your types "compiled" into a
java jar and that jar must be listed in your `CLASSPATH` for `zcm-spy` to be able
to decode messages.

### Spy Lite
##### To mark for build: `$./waf configure --use-elf`

Sometimes developers don't have access to a display environment to run `zcm-spy`.
The terminal-based `zcm-spy-lite` is provided for exactly that situation.
`zcm-spy-lite` is a lite version of zcm spy that runs entirely from the terminal.
You can view message traffic and inspect message data all in a display-less
environment. To use `zcm-spy-lite` you need to tell it where to listen for messages
and where it can find a shared library containing the zcmtypes you would like it to
be able to decode. For an example on how to compile the shared library see the example further down.


### Logger
##### To mark for build: `$./waf configure --use-elf`

It is often desirable to record the messaging data events and record them for later
debugging. On a robotics system this is very important because often the developer
cannot debug in real-time nor deterministically reproduce bugs that have previously
occurred. By recording live events, debugging can be done after an issue occurs.
ZCM ships with a built-in logging API using `zcm/eventlog.h`. ZCM also provides
a stand-alone process `zcm-logger` that records all events it receives on the
specified transport.

### Log Player

After capturing a ZCM log, it can be *replayed* using the `zcm-logplayer` tool.
This tool republishes the events back onto a ZCM transport. For here, any ZCM
subscriber application can receive the data exactly as it would have live! This
tool, combined with the logger creates a powerful development approach for
systems with limited debug-ability. `zcm-logplayer` also allows for playback
directly into an output log file. This might seem strange at first since `cp`
would accomplish the same end goal; however, `zcm-logplayer` also comes with a
meta file interface indicated by a file with a `.jslp` file extension. This file
specifies certain playback behavior. Currently supported functionality includes
the ability to whitelist, blacklist, or specify certain channels to be played
back. Other functionality includes the ability to only start playback of the
log after a certain amount of time has elapsed, or after the first message has
been broadcasted on a specified channel. A definition of a `jslp` file follows:

    {
        "FILTER" : {
            "type"     : "channels",
            "mode"     : "whitelist" | "blacklist" | "specified",
            "channels" : { "MODEL" : false }
        },
        "START" : {
            "mode"    : "channel" | "us_delay",
            "channel" | "us_delay" : "COMMAND_1_RX" | number_of_us
        }
    }

see examples/tools/logplayer/example.log.jslp for more examples.

### Log Player GUI
##### To mark for build: `$./waf configure --use-java`

This is similar to `zcm-logplayer` but is a GUI-based tool. Launch `zcm-logplayer-gui`
to play back a log interactively. Speed up/slow down playback, play/pause,
scrub through the log, make bookmarks, mark sections to play on repeat,
export log snippets, all from one lightweight and easy to use tool.


### Bridge
##### To mark for build: `$./waf configure --use-elf`

When architecting a system that uses zcm, you might want to use multiple
transports. `zcm-bridge` allows you to bridge traffic between two
transports essentially subscribing to traffic on one transport, republishing it
on another, and vice versa

### Repeater

`zcm-repeater` is almost identical to `zcm-bridge` but is unidirectional.
It takes traffic on one transport and channel and rebroadcasts it to a
new channel

### Spy Peek

`zcm-spy-peek` is an extremely lightweight tool that simply prints when a message
is received. It's intended to be a sanity check tool to help diagnose when issues
arrise sending data from point A to point B. Running `zcm-spy-peek` while also
running the example publisher would result in the following printing to the screen:

    Message received on channel: "EXAMPLE"
    Message received on channel: "EXAMPLE"
    Message received on channel: "EXAMPLE"
    ...

<!-- ADD MORE HERE -->

## ZCM Tools Example

For this example we'll build a small application which will count up from zero and
publish its new value on a channel named "COUNT".
First we need to define the ZCM message type for counting (count\_t.zcm):

    struct count_t
    {
        int32_t val;
    }

Now because we want `zcm-spy-lite` to be able to decode our message, we need
to generate the bindings with extra type information:

    zcm-gen -c count_t.zcm --c-typeinfo

The `--c-typeinfo` flag is to include type introspection in the output zcmtype
source files. This means that auto-gen functions are included in the output type
to allow `zcm-spy-lite` to lookup the name and fields of the type from that type's
hash. It is only recommended if plan on using `zcm-spy-lite`. If you need to save
on size or if you don't care to use `zcm-spy-lite`, you can omit this flag.

Next up we need to write the source code for the publisher application itself (publish.c):

    #include <unistd.h>
    #include <zcm/zcm.h>
    #include <count_t.h>

    int main(int argc, char *argv[])
    {
        zcm_t *zcm = zcm_create("ipc");
        count_t cnt = {0};

        while (1) {
            count_t_publish(zcm, "COUNT", &cnt);
            cnt.val++;
            usleep(1000000); /* sleep for a second */
        }

        zcm_destroy(zcm);
        return 0;
    }

Building and running the publisher:

    cc -o publish -I. publish.c count_t.c -lzcm
    ./publish

Building the shared library for `zcm-spy-lite`

    cc -c -fpic count_t.c
    cc -shared -o libzcmtypes.so count_t.o

Now that you have a shared library of your zcmtypes, you need to point
`zcm-spy-lite` to the folder where that library is stored. Much like
LD\_LIBRARY\_PATH is used to help your system find the paths to shared libraries
it needs to run programs, ZCM\_SPY\_LITE\_PATH is used to point `zcm-spy-lite`
to the shared library.

    ZCM_SPY_LITE_PATH=<full path to shared library>

To make that environment variable load each time you open a new terminal, you
can add it to the bottom of your shell profile. For bash this would be adding
the following line to the bottom of your ~/.bashrc

    export ZCM_SPY_LITE_PATH=<full path to shared library>

We can now *spy* on the ZCM traffic with:

    zcm-spy-lite --zcm-url ipc

This will show an overview of all ZCM channels `zcm-spy-lite` have received
messages on. In this example only one channel is in use, but in real applications
many tens of channels might be in use at once.
For each channel you can see the total amount of received messages and the current
frequency. To inspect the messages of a specific channel press `-` and type the
number left of the channel name. For quick access to the first 9 channels, just
press the number on your keyboard directly. You will now be able to see the last
message transmitted on the channel. To go back to the overview press `ESC`

Instead of monitoring the messages in real-time you can also record them for later
review or playback using the `zcm-logger`. To record the ZCM messages for a few seconds:

    zcm-logger --zcm-url ipc

This will produce a ZCM log file in the current directory
named with the pattern: `zcmlog-{YEAR}-{MONTH}-{DAY}.00`

We can *replay* these captured events using the `zcm-logplayer` tool:

    zcm-logplayer --zcm-url ipc zcmlog-*.00

The replay tool alone is not very interesting until we combine it
with another application that will receive the data. For this purpose
we can use the `zcm-spy-lite` tool, running it before the replay tool:

    zcm-spy-lite --zcm-url ipc &
    zcm-logplayer --zcm-url ipc zcmlog-*.00




## Advanced Tools

### CsvWriter
##### To mark for build: `$./waf configure --use-elf`

Sometimes it is useful to convert either a zcmlog or live zcm data into csv format.
This tool, launched via

    zcm-csv-writer

does this for you. See command line options for how to customize the output.

### CsvReader
##### To mark for build: `$./waf configure --use-java`

Sometimes it is useful to convert a csv into a zcmlog.
This tool, launched via

    zcm-csv-reader

does this for you. There is currently no default format that this reader will
be able to read in, however you may write your own CsvReaderPlugin for
custom csv parsing. Examples are provided in the examples directory in zcm.

### Transcoder
##### To mark for build: `$./waf configure --use-elf`

As explained in [type generation](zcmtypesys.md), modifying a zcmtype changes
that types hash and therefore invalidates all old logs you may have. However,
sometimes it may be desirable to add a field to a type without invalidating all
prior logs. To do this we provide a log transcoder launched via

    zcm-log-transcoder

and the TranscoderPlugin interface so you may define the mapping from old log
to new log. This tool can even let you convert between completely different types

### Indexer
##### To mark for build: `$./waf configure --use-elf`

This tool was designed to make programmatically working with zcm logs faster.
The purpose of this tool is best explained with an example.

Let's say that you have a log taken from a submersible ROV. Included in the log are messages
containing the beacon localization coordinates of the robot, and images the robot took
throughout the mission.

So what do you do if you want to extract all of the images that are 10m or more below the
surface? And better yet, what if you want them to be sorted in height order?

Well assuming you've gone through the tutorial, you already know how to programmatically
subscribe to zcm traffic; you could write a quick python/java/c/c++/{insert favorite
supported language here} program to subscribe to data and do the matching or filtering,
compile and run your program, and then use `zcm-logplayer` to play your log back while
your program is running.

If you were brave enough to dive into the code base already, you might already know about
the zcm log support in each language, and you'd be able to write a program that opens the log
and works with it directly (sans subscriptions). You would write a script in the language of
your choice that opens the log and attempts to match coordinate messages to picture
messages based on the times at which the measurements were taken. You would then filter
out all pairs that are above 10m deep, and finally sort the data in ascending depth order.

But what if you don't want to replicate information that's already stored in your log?
Better yet, what if you're not sure what you're going to want to do with the data
in your log? What if you're on an analytics team where each member is playing with different
data from the log? Do you replicate the log once for each member?

Sure, why not!

That answer is fine while your logs are small in size and number, but what if your
team is analyzing thousands of logs that are each gigabytes large?
Time and space become of the essence.

Let's go back to the previous example. However this time, instead of extracting the
image messages with the desired characteristics from the log, let's just save
their index in the log. Think of a log as a giant array of events. If we want to
remember that event number 46 is of importance, we store the number 46 in our index file.
When we pair the index information with the log itself, we have the ability to
directly jump to specific events in the log.

This is where `zcm-log-indexer` comes in.
`zcm-log-indexer` implements the code that opens the log, filters and
sorts the content events, and outputs a file containing the offset index
(encoded in json format). By default, `zcm-log-indexer` outputs an index file
that contains the offsets of each message type in the log sorted in timestamp order.
An interface is exposed so you can provide "plugins" to the indexer tool that
specify other ways you'd like logs to be indexed. The indexer will index logs by
every available plugin. Take a look at `zcm/tools/IndexerPlugin.hpp` for the plugin
interface and for an example custom plugin.

So let's go ahead and use `zcm-log-indexer`. But this time, let's use a simpler example.
In the case of a logfile taken by our ROV, we might want to extract all images in the log
in timestamp order. But we don't want to crawl through the log looking for image messages.
Assuming our log file is called `zcm.log`, we run the following command:

    zcm-log-indexer -l zcm.log -o zcm.dbz -t types.so -r

Note that the `-r` flags makes the output `zcm.dbz` file readable for humans.
After running the above command, the output index file might look like this:

    {
        "timestamp" : {
            "IMAGES" : {
                "image_t" : [
                    "0",
                    "1001000",
                    "2002000",
                    ...
                    "37037000"
                ]
            },
            "BEACON_COORDS" : {
                "beacon_t" : [
                    "1000000",
                    "1000100",
                    "1000200",
                    "1000300",
                    ...
                    "37036900"
                ]
            }
        }
    }

Notice that the file is first sorted by plugin name. This is the standard behavior
of `zcm-log-indexer`. Each plugin specifies its "name" as part of it's implementation.
The output of that plugin is always held in a high level json object whose key is
the plugin's name. In this case, the plugin's name is "timestamp".
After that point, the plugin specifies the rest of the organization of its json
index object. In the default case, the timestamp plugin organizes its output first
by channel name, then by zcm type name, but custom plugins are free to organize as
they see fit. The API through which custom plugins specify their organization is
specified in the base `IndexerPlugin.hpp` class. See that file for more information.

Now that we have both the zcm log and this index file, we can use it in whatever
zcm-supported language we please. Let's write a quick python script to print the times
of each image in our index in the order provided by the index.

    import sys
    sys.path.insert(0, './build/types/')
    from image_t import image_t

    from zerocm import ZCM, LogFile, LogEvent
    log = LogFile('zcm.log', 'r')

    import json
    with open('zcm.dbz') as indexFile:
        index = json.load(indexFile)

    i = 0
    while i < len(index['timestamp']['IMAGES']['image_t']):
        evt = log.readEventOffset(int(index['timestamp']['IMAGES']['image_t'][i]))
        image = image_t.decode(evt.getData())
        print image.name + ": " + str(image.timestamp)


If you're still confused as to exactly how to use the tool, that's expected.
Head on over to the `examples` part of the repo and take a look at a custom plugin
in `examples/cpp/CustomIndexerPlugin.cpp` and then how to use it to quickly
traverse logs in `examples/python/indexer_test.py`

When working with custom plugins, your launch command might looks like so:

    zcm-log-indexer -l zcm.log -o zcm.dbz -t types.so -p plugins.so

To tell `zcm-log-indexer` about your custom plugins and zcmtypes, you simply
compile a shared library and pass it to the tool via a command line argument.
You can also use the environment variables mentioned in the `--help` section
of `zcm-log-indexer` for specifying the `types.so` and `plugins.so` libraries.
Compiling a shared library is as easy as:

    g++ -std=c++11 -fPIC -shared CustomPlugin.cpp -o plugins.so

### Filter
##### To mark for build: `$./waf configure --use-elf`

Sometimes you may want to filter a zcm log down to only certain events.
To do this, we provide a tool that allows you to define on regions of a log
that you would like to keep.
This tool, launched via

    zcm-log-filter

does this by providing a rich command line interface through which you can
specify multiple regions with complex begin and end conditions.
Refer to the usage docs (`zcm-log-filter -h`) for the most up-to-date usage.
