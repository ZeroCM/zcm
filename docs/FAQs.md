<a style="margin-right: 1rem;" href="javascript:history.go(-1)">Back</a>
[Home](../README.md)
# Frequently Asked Questions


### In trying to run the publish test project from the tutorial, i get the error `./publish: error while loading shared libraries: libzcm.so: cannot open shared object file: No such file or directory`

If you see this error when trying to use anything zcm related, then you probably have a problem with your LD\_LIBRARY\_PATH environment variable. Try echoing $LD\_LIBRARY\_PATH and verifying that it points to `/usr/lib:/usr/local/lib`. If it doesn't (which is weird), point it there yourself with:

    export LD_LIBRARY_PATH=/usr/lib:/usr/local/lib

Add that line to the bottom of your ~/.bashrc to make it permanent.


### In trying to run zcm-spy or another java program I get the following error:

    Exception in thread "main" java.lang.UnsatisfiedLinkError: no zcmjni in java.library.path
            at java.lang.ClassLoader.loadLibrary(ClassLoader.java:1867)
            at java.lang.Runtime.loadLibrary0(Runtime.java:870)
            at java.lang.System.loadLibrary(System.java:1122)
            at zcm.zcm.ZCMJNI.<clinit>(ZCMJNI.java:7)
            at zcm.zcm.ZCM.<init>(ZCM.java:37)
            at zcm.spy.Spy.<init>(Spy.java:77)
            at zcm.spy.Spy.main(Spy.java:481)

This is a similar symptom to the first question. It means your $LD\_LIBRARY\_PATH isn't set correctly. See the answer to the question above.



### I'm trying to run my program and ZCM's not working and I don't know why

Have you tried running your program with `ZCM_DEBUG=1` in front of it? This enables debug output from zcm which should help you diagnose your problems



### How does ZCM know where one serial message ends and which ZCM type should decode it?

Use the `serial://` transport instead of writing raw concatenated message bytes to the device. The default serial transport wraps each ZCM message in a frame that contains the channel name, payload length, and checksum. On receive, ZCM scans the byte stream for the frame marker, validates the checksum, and then delivers the reconstructed `(channel, payload)` message to subscribers.

The channel is the discriminator that lets your application decide which generated message type to decode. Subscribe to the channels you expect and decode each delivered payload with the matching generated type. If you have many message types on the same radio or UART link, publish them on distinct channels such as `GPS`, `COMPASS`, and `IMU`; receivers subscribe to those channels and ZCM routes the framed messages accordingly.

For noisy byte-stream links, the serial framing can re-synchronize after corrupt bytes because each frame has a marker and checksum. For messages larger than the serial MTU, enable packetized serial mode with `pkt_size`, for example `serial:///dev/ttyUSB0?baud=115200&pkt_size=128`.

See also [Transport Layer](transports.md), [Generic Serial Transport](generic_serial_transport.md), and [Packetized Serial Transport](packetized_serial_transport.md).


### Why does my program freeze when I try to subscribe / unsubscribe from within a callback?

Calling the subscribe / unsubscribe functions modifies the same internal data structures as
the dispatch code reads during callbacks. Due to a lock, calling these functions from within a
callback causes a deadlock. You should therefore alter the subscriptions outside of your callbacks.
If your code really needs to change subscriptions in response to a received message, try using a queue
to pass the work to another thread or in your main-loop if using `zcm_handle`.



### In NodeJS, why do my server-side subscriptions randomly stop working or segfault?
**This is no longer an issue and is addressed on ZCM master**

NodeJS' garbage collector will come around and clean up a subscription unless
that subscription is referenced elsewhere in your code. The easiest way to get
around this problem is by calling unsubscribe on all your subscriptions on process.exit

    process.on('exit', function() { z.unsubscribe(sub); });
