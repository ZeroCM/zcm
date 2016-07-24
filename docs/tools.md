# ZCM Tools

ZCM ships with a convenient set of debugging and monitoring tools. In this section
we demonstrate the usage of these tools.

### Spy

Since ZCM makes data decoupling so easy, developers tend to build applications in
several modules/processes and tie them together with ZCM message passing. In this
model, a lot of the debugging takes place at the message-passing level. Often
it's desirable to inspect/spy the messages in transit. This can be accomplished
using the `zcm-spy` tool. Note that you must have your types "compiled" into a
java jar and that jar must be listed in your `CLASSPATH` for `zcm-spy` to be able
to decode messages.

### Spy Lite

Sometimes developers don't have access to a display environment to run `zcm-spy`.
The terminal-based `zcm-spy-lite` is provided for exactly that situation.
`zcm-spy-lite` is a lite version of zcm spy that runs entirely from the terminal.
You can view message traffic and inspect message data all in a display-less
environment. To use `zcm-spy-lite` you need to tell it where to listen for messages
and where it can find a shared library containing the zcmtypes you would like it to
be able to decode. For an example on how to compile the shared library see the example further down.




### Logger

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
systems with limited debug-ability.

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

Sometimes it is useful to convert either a zcmlog or live zcm data into csv format.
This tool, launched via

    zcm-csv-writer

does this for you. There is a default format that this writer will output in,
however often times, it is more useful to write your own CsvWriterPlugin for
custom output formatting. Examples are provided in the examples directory in zcm.

### CsvReader

Sometimes it is useful to convert a csv into a zcmlog.
This tool, launched via

    zcm-csv-reader

does this for you. There is currently no default format that this reader will
be able to read in, however you may write your own CsvReaderPlugin for
custom csv parsing. Examples are provided in the examples directory in zcm.

### Transcoder

As explained in [type generation](zcmtypesys.md), modifying a zcmtype changes
that types hash and therefore invalidates all old logs you may have. However,
sometimes it may be desirable to add a field to a type without invalidating all
prior logs. To do this we provide a log transcoder launched via

    zcm-log-transcoder

and the TranscoderPlugin interface so you may define the mapping from old log
to new log. This tool can even let you convert between completely different types
