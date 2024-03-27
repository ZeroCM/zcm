<a style="margin-right: 1rem;" href="javascript:history.go(-1)">Back</a>
[Home](../README.md)
# Transport Layer

The transport layer is a very important feature for ZCM. With
this subsystem, end-users can craft their own custom transport implementations
to transfer zcm messages. This is achieved with a generic transport interface
that specifies the services a transport must provide and how ZCM will use
a given transport.

## Built-in Transports

With this interface contract, we can implement a wide set of transports.
Many common transport protocols have a ZCM variant built-in to the library.
The following table shows the built-in transports and the URLs that can
be used to *summon* the transport:

<table>
  <thead><tr>
    <th>        Type          </th>
    <th>        URL Format    </th>
    <th>        Example Usage </th>
  </tr></thead>
  <tr>
    <td>        Inter-thread                                            </td>
    <td><code>  inproc                                                  </code></td>
    <td><code>  zcm_create("inproc"), zcm_create("inproc://mysubnet")   </code></td>
  </tr>
  <tr>
    <td>        Inter-process (IPC)                                     </td>
    <td><code>  ipc://&lt;ipc-subnet&gt;                                </code></td>
    <td><code>  zcm_create("ipc"), zcm_create("ipc://mysubnet")         </code></td>
  </tr>
  <tr>
    <td>        Nonblocking Inter-thread                                </td>
    <td><code>  nonblock-inproc                                         </code></td>
    <td><code>  zcm_create("nonblock-inproc")                           </code></td>
  </tr>
  <tr>
    <td>        UDP Multicast                                           </td>
    <td><code>  udpm://&lt;ipaddr&gt;:&lt;port&gt;?ttl=&lt;ttl&gt;      </code></td>
    <td><code>  zcm_create("udpm://239.255.76.67:7667?ttl=0")           </code></td>
  </tr>
  <tr>
    <td>        UDP Unicast                                                            </td>
    <td><code>  udp://&lt;ipaddr&gt;:&lt;sub_port&gt;:&lt;pub_port&gt;?ttl=&lt;ttl&gt; </code></td>
    <td><code>  zcm_create("udp://127.0.0.1:9000:9001?ttl=0")                          </code></td>
  </tr>
  <tr>
    <td>        Serial                                                  </td>
    <td><code>  serial://&lt;path-to-device&gt;?baud=&lt;baud&gt;       </code></td>
    <td><code>  zcm_create("serial:///dev/ttyUSB0?baud=115200")         </code></td>
  </tr>
  <tr>
    <td>        CAN                                                     </td>
    <td><code>  can://&lt;interface&gt;?msgid=&lt;id&gt;                </code></td>
    <td><code>  zcm_create("can://can0?msgid=65536")                    </code></td>
  </tr>
  <tr>
    <td>        Inter-process via Shared Memory (IPCSHM)                </td>
    <td><code>  ipcshm://&lt;shm-region-name&gt;?mtu=&lt;mtu&gt;&amp;depth=&lt;depth&gt&amp;mlock=&lt;1 or 0&gt; </code></td>
    <td><code>  zcm_create("ipcshm"), zcm_create("ipcshm://myregion?mtu=100000&depth=128&mlock=1")   </code></td>
  </tr>
</table>

When no url is provided (i.e. `zcm_create(NULL)`), the `ZCM_DEFAULT_URL` environment variable is
queried for a valid url.

## Custom Transports

While these built-in transports are enough for many applications, there are many situations
that can benefit from a custom transport protocol. For this, we can use the transport API.
In fact, the transport API is *first-class*. All transports, even built-in ones, use this API.
There is nothing special about the built-in transports. You can even configure a custom transport
to use a URL just like above.

The transport API is a C89-style interface. While it is possible to expose this interface to
other languages, we don't currently provide this capability. However, you can still implement a
custom transport using the C89 API and access it from any other language just like the built-ins.

There are two variants of this interface. One variant is for implementing blocking-style transports,
and the other is for non-blocking transports. In most cases on a linux system, the blocking
interface is most convenient, but the non-blocking interface can also be used if desired.

All of the core transport code is contained in `zcm/transport.h` and reading it is
highly recommended.

### Core Datastructs

To pass message data between the core-library and the transport implementations, the
following datastruct is employed:

    struct zcm_msg_t
    {
        uint64_t utime;  /* 0 means invalid (caller should compute its own utime) */
        const char *channel;
        size_t len;
        char *buf;
    };

To implement a polymorphic interface with only C89 code, we use a hand-rolled virtual-table
of function pointers to the type. The following struct represents this virtual-table:

    struct zcm_trans_methods_t
    {
        size_t  (*get_mtu)(zcm_trans_t *zt);
        int     (*sendmsg)(zcm_trans_t *zt, zcm_msg_t msg);
        int     (*recvmsg_enable)(zcm_trans_t *zt, const char *channel, bool enable);
        int     (*recvmsg)(zcm_trans_t *zt, zcm_msg_t *msg, int timeout);
        int     (*update)(zcm_trans_t *zt);
        void    (*destroy)(zcm_trans_t *zt);
    };

To make everything work, we need a *basetype* that is aware of the virtual-table and understands
whether it is a blocking or non-blocking style transport. Here is this type:

    struct zcm_trans_t
    {
        enum zcm_type trans_type;
        zcm_trans_methods_t *vtbl;
    };

Creating custom transports requires a bit of pointer coercion, but otherwise is fairly straightforward. Here's is an outline of a typical implementation:

    typedef struct
    {
        enum zcm_type trans_type;
        zcm_trans_methods_t *vtbl;
        /* transport specific data here */
    } my_transport_t;

    /* implement the API methods here:
        my_transport_get_mtu
        my_transport_sendmsg
        my_transport_recvmsg_enable
        my_transport_recvmsg
        my_transport_update
        my_transport_destroy
    */

    static zcm_trans_methods_t methods = {
        my_transport_get_mtu,
        my_transport_sendmsg,
        my_transport_recvmsg_enable,
        my_transport_recvmsg,
        my_transport_update,
        my_transport_destroy
    };

    zcm_trans_t *my_transport_create(zcm_url_t *url)
    {
        my_transport_t *trans;
        /* construct trans here */

        trans->trans_type = ZCM_BLOCKING;  /* or ZCM_NONBLOCKING */
        trans->vtbl = &methods;            /* setting the virtual-table defined above */

        return (zcm_trans_t *) trans;
    }

IMPORTANT: The `my_transport_t` struct **must** perfectly mirror the start of `zcm_trans_t`.

IMPORTANT: The `my_transport_create` **must** set the `trans_type` and `vtbl` fields


### Blocking Transport API Semantics

 -  `size_t get_mtu(zcm_trans_t *zt)`

   Returns the Maximum Transmission Unit supported by this transport.
   The transport is allowed to ignore any message above this size.
   Users of this transport should ensure that they never attempt to
   send messages larger than the MTU of their chosen transport.

 -  `int sendmsg(zcm_trans_t *zt, zcm_msg_t msg)`

   The caller to this method initiates a message send operation. The
   caller must populate the fields of the `zcm_msg_t`. The channel must
   be less than `ZCM_CHANNEL_MAXLEN` and `len <= get_mtu()`, otherwise
   this method can return `ZCM_EINVALID`. On receipt of valid params,
   this method should block until the message has been successfully
   sent and should return `ZCM_EOK`.

 -  `int recvmsg_enable(zcm_trans_t *zt, const char *channel, bool enable)`

   This method will enable/disable the receipt of messages on the particular
   channel. This method is like a "suggestion", the transport is allowed to
   "enable" more channels without concern. This method only sets the
   "minimum set" of channels that the user expects to receive. It exists to
   provide the transport layer more information for optimization purposes
   (e.g. the transport may decide to send each channel over a different endpoint).

   NOTE: This method should work concurrently and correctly with `recvmsg()`.
   On success, this method should return `ZCM_EOK`

   NOTE: `channel` may be a regex character string.
   See [the announcement](announcements/transport_recvmsg_enable_change.md)
   about this topic for more information

 - `int recvmsg(zcm_trans_t *zt, zcm_msg_t *msg, int timeout)`

   The caller to this method initiates a message recv operation. This
   methods blocks until it receives a message. It should return `ZCM_EOK`.
   Messages that have been *enabled* with `recvmsg_enable()` *must* be received.
   Extra messages can also be received; the *enabled* channels define a minimum set.

   If `timeout >= 0` then `recvmsg()` should return `ZCM_EAGAIN` if it is unable
   to receive a message within the allotted time.

   NOTE: We do **NOT** require a very accurate clock for this timeout feature
   and users should only expect accuracy within a few milliseconds. Users
   should **not** attempt to use this timing mechanism for real-time events.

   NOTE: This method should work concurrently and correctly with
   `recvmsg_enable()`.

 - `int update(zcm_trans_t *zt)`

   This method is unused (in this mode) and should not be called by the user.
   An implementation is allowed to set this vtable field to NULL.

 - `void destroy(zcm_trans_t *zt)`

   Close the transport and cleanup any resources used.

### Non-blocking API Semantics

General Note: None of the non-blocking methods must be thread-safe.
This API is designed for single-thread, non-blocking, and minimalist
transports (such as those found in embedded). In order to make nonblocking
transports friendly to embedded systems, memory for subscriptions is
allocated at compile time. You can control the maximum number of
subscriptions by defining the preprocessor variable, `ZCM_NONBLOCK_SUBS_MAX`.
By default, this number is 512.

 - `size_t get_mtu(zcm_trans_t *zt)`

   Returns the Maximum Transmission Unit supported by this transport.
   The transport is allowed to ignore any message above this size.
   Users of this transport should ensure that they never attempt to
   send messages larger than the MTU of their chosen transport.

 - `int sendmsg(zcm_trans_t *zt, zcm_msg_t msg)`

   The caller to this method initiates a message send operation. The
   caller must populate the fields of the `zcm_msg_t`. The channel must
   be less than `ZCM_CHANNEL_MAXLEN` and `len <= get_mtu()`, otherwise
   this method can return `ZCM_EINVALID`. On receipt of valid params,
   this method should **never block**. If the transport cannot accept the
   message due to unavailability, `ZCM_EAGAIN` should be returned. A transport
   implementing this function will typically buffer the entirety of a message
   into an internal buffer that will be flushed out upon successive calls to
   `update()` below. On success `ZCM_EOK` should be returned.

 - `int recvmsg_enable(zcm_trans_t *zt, const char *channel, bool enable)`

   This method will enable/disable the receipt of messages on the particular
   channel. For *all channels*, the user should pass NULL for the channel.
   This method is like a "suggestion", the transport is allowed to "enable"
   more channels without concern. This method only sets the "minimum set"
   of channels that the user expects to receive. It exists to provide the
   transport layer more information for optimization purposes (e.g. the
   transport may decide to send each channel over a different endpoint).

   NOTE: This method does NOT have to work concurrently with `recvmsg()`.
   On success, this method should return `ZCM_EOK`

 - `int recvmsg(zcm_trans_t *zt, zcm_msg_t *msg, int timeout)`

   The caller to this method initiates a message receive operation. This
   methods should **never block**. If a message has been received then
   `ZCM_EOK` should be returned, otherwise it should return `ZCM_EAGAIN`.
   Messages that have been *enabled* with `recvmsg_enable()` **must** be received.
   Extra messages can also be received; the *enabled* channels define a minimum set.

   NOTE: This method does NOT have to work concurrently with `recvmsg_enable()`

   NOTE: The timeout field is ignored

 - `int update(zcm_trans_t *zt)`

   This method is called from the `zcm_handle_nonblock()` function.
   This method provides a periodically-running routine that can perform
   updates to the underlying hardware or other general maintenance to
   this transport. A transport implementing this function will typically use
   this time to flush out any bytes left in its internal buffer. This method
   should **never block**. Again, this method is called from
   `zcm_handle_nonblock()` and thus runs at the same frequency as
   `zcm_handle_nonblock()`. Failure to call `zcm_handle_nonblock()`
   while using an nonblock transport may cause the transport to work
   incorrectly on both message send and recv.

 - `void destroy(zcm_trans_t *zt)`

   Close the transport and cleanup any resources used.

### Registering a Transport

Once we've implemented a new transport, we can *register* its create function with ZCM.
Registering a transport allows users to create it using a url just like the built-in transports.
To register, we simply need to call the following function (from `zcm/transport_registrar.h`):

    bool zcm_transport_register(const char *name, const char *desc, zcm_trans_create_func *creator);

The `name` parameter is the simply the name of the transport protocol to use in the url.

If we wanted to register the transport we created above, we could write:

    zcm_transport_register("myt", "A custom example transport", my_transport_create);

Now, we can create new `my_transport` instances with:

    zcm_create("myt://endpoint-for-my?param1=abcd&param2=dfg&param3=yui");

This will issue a call to our `my_transport_create` function with a `zcm_url_t`

We even can go a step further and register transports in a static-context (i.e. before main)!
This can be achieved we either a compiler-specific attribute (in C) or with a static object
constructor in C++. The `zcm_transport_register` function is designed to be static-context safe.
To learn more about how to do this in C++, take a look at `zcm/transport/transport_zmq_local.cpp`

Lastly, ZCM supports using a transport without registering. In this mode, URL parsing is
not supported. Instead, the user manually constructs a custom `zcm_trans_t*` and passes
it directly to the core library with the following function:

    zcm_t *zcm_create_from_trans(zcm_trans_t *zt);

## Blocking vs Non-Blocking Message Handling

In most ways, the distinction between blocking and non-blocking transports are completely
transparent to the end user. Internally, ZCM operates differently depending on the type
of transport it's using. This allows all of the following to work *identically*:

  - `zcm_publish`
  - `zcm_subscribe`
  - `zcm_unsubscribe`
  - Message handler callbacks

However, like all abstractions, this one is not completely *leak-free*. Despite our best efforts,
we were unable to design a reasonable approach to message dispatching. Rather than try to force
a broken abstraction that would behave differently depending on mode, we opted to make this
difference explicit with distinct API methods.

For the blocking case, there are the following approaches:

  - Spawning a thread
    - `zcm_start()     /* starts a dispatch thread to be the mainloop */`
  - Becoming the mainloop
    - `zcm_run()       /* this thread becomes the mainloop and doesn't return */`
  - LCM-style (only for API compatibility)
    - `zcm_handle()    /* block for a message, dispatch it, and return */`
  - Cleanup
    - `zcm_stop()      /* stops the all threads, even those not used in message dispatching */`

For the non-blocking case, there is a single approach:

  - `zcm_handle_nonblock()  /* returns non-zero if a message was available and dispatched */`

To prevent errors, the internal library checks that the API method matches the transport type.

## Closing Thoughts

While incredibly powerful, the transport API can be tricky to understand and use as a beginner.
This appears to primarily be the incidental complexity of our requirements:

  - C89 API for supporting embedded (and other languages in the future)
  - Blocking and Non-blocking support
  - First-class usage (no special built-in transports)

For the aspiring transport developer, we recommend reading the following core sources:

  - `zcm/transport.h`
  - `zcm/transport_registrar.h` and `zcm/transport_registrar.c`

It's also great to browse the implementations of built-in transports:

  - `zcm/transport/transport_zmq_local.cpp`
  - `zcm/transport/transport_serial.cpp`
  - `zcm/transport/udp/udp.cpp`

Finally, we love contributions! Check out [Contributing](contributing.md).

<hr>
<a style="margin-right: 1rem;" href="javascript:history.go(-1)">Back</a>
[Home](../README.md)
