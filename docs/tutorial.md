<a style="margin-right: 1rem;" href="javascript:history.go(-1)">Back</a>
[Home](../README.md)
# ZCM Step-by-Step Tutorial

We believe the best way to learn new things is to just dive in and start making mistakes.
It's only by making those early mistakes that one truly can understand the folly of one's
ways and to grow as a result. So, without further ado, let's get started.

## Basic Types

To understand ZCM, we must start by discussing its message type-system. Every message sent inside
ZCM can be tied back to some message specification. Let's explore this with a simple, yet practical example.
Imagine that we have a typical rotation sensor such as an IMU or Gyro. Our sensor produces
yaw, pitch, and roll measurements as well as a few flags describing it's mode of operation. If we were
programming in just C code, we might design a struct type as follows:

    struct rotation_t
    {
        double yaw;
        double pitch;
        double roll;
        int   flags;
    };

In ZCM, we aim to retain this rich data structuring and layout while still being able to do
powerful message-passing in numerous different programming languages. To do this, we use a
programming-language-agnostic type specification language. Fortunately for our example above,
it is very similar to C struct syntax. Here it is:

    struct rotation_t
    {
        double yaw;
        double pitch;
        double roll;
        int32_t flags;
    }

The ZCM type is almost an exact copy. The key difference here is that in C 'int' is not
strictly defined. An 'int' can be a different size depending on the machine the software
runs on. Because a ZCM Type is a binary data-exchange format, it needs to have rigourously
defined types that mean the same thing on each machine. Here's a brief table of the default
built-in types:

<table>
    <tr><td>  int8_t   </td><td>  8-bit signed integer              </td></tr>
    <tr><td>  int16_t  </td><td>  16-bit signed integer             </td></tr>
    <tr><td>  int32_t  </td><td>  32-bit signed integer             </td></tr>
    <tr><td>  int64_t  </td><td>  64-bit signed integer             </td></tr>
    <tr><td>  float    </td><td>  32-bit IEEE floating point value  </td></tr>
    <tr><td>  double   </td><td>  64-bit IEEE floating point value  </td></tr>
    <tr><td>  string   </td><td>  UTF-8 string                      </td></tr>
    <tr><td>  boolean  </td><td>  true/false logical value          </td></tr>
    <tr><td>  byte     </td><td>  8-bit value                       </td></tr>
</table>

Believe it or not, this is all the basic knowledge needed to get started. Let's dive into
code, and build a working program using ZCM!

## Hello World

Let's build a canonical Hello World message-passing program on top of ZCM. For this we
will need two programs. One program will create and *publish* new messages and the
other program will *subscribe* to and receive those messages.

### Hello World Types

First, let's create a new zcm type in the file *msg_t.zcm*

    struct msg_t
    {
        string str;
    }

We can now use the *zcm-gen* tool to convert this type specification into bindings
for many different programming languages. We will be using C code for this example, so run:

    zcm-gen -c msg_t.zcm

This command will produce two new files in the current directory (msg\_t.h and msg\_t.c).
Feel free to browse these files for a moment to get a rough idea of how zcm-gen works. Notice
that zcm-gen has converted the *string* to a native C type. ZCM always tries to use the
native data types in each programming langauge, so the programming experience doesn't feel foreign.

    typedef struct _msg_t msg_t;
    struct _msg_t
    {
         char*      str;
    };

### Hello World Publish

For most simple programs, the only include needed is the basic zcm, but we also need to include our msg_t header

    #include <zcm/zcm.h>
    #include <msg_t.h>

In the main function we have to create a zcm instance that will manage the communications. Here, you decide which
transport protocol to use and provide an appropriate url. For this example, we'll use Inter-process Communication (IPC):

    zcm_t *zcm = zcm_create("ipc");

Finally, we simply construct a msg\_t and then publish it repeatedly:

    msg_t msg;
    msg.str = (char*)"Hello, World!";

    while (1) {
        msg_t_publish(zcm, "HELLO_WORLD", &msg);
        usleep(1000000); /* sleep for a second */
    }

And that's it! Here's the full program (publish.c):

    #include <unistd.h>
    #include <zcm/zcm.h>
    #include <msg_t.h>

    int main(int argc, char *argv[])
    {
        zcm_t *zcm = zcm_create("ipc");

        msg_t msg;
        msg.str = (char*)"Hello, World!";

        while (1) {
            msg_t_publish(zcm, "HELLO_WORLD", &msg);
            usleep(1000000); /* sleep for a second */
        }

        zcm_destroy(zcm);
        return 0;
    }


Building and running:

    cc -o publish -I. publish.c msg_t.c -lzcm
    ./publish

If you have the java ZCM tools installed, you should be able to run `zcm-spy` and
see the published messages now!

    zcm-spy --zcm-url ipc

Notice how you are unable to decode the messages inside of `zcm-spy`, as it does not know
your message types. For a guide on properly use zcm-spy and the rest of the zcm tooling see
[Tools](tools.md)

### Hello World Subscribe

Let's build a program to receive those messages. We'll start off the same as before
by adding the headers and creating a new zcm_t\* instance. Then, we need to *subscribe*
to the particular channel. But, first we need to create a callback function to handle all the
received messages:

    void callback_handler(const zcm_recv_buf_t *rbuf, const char *channel, const msg_t *msg, void *usr)
    {
        printf("Received a message on channel '%s'\n", channel);
        printf("msg->str = '%s'\n", msg->str);
        printf("\n");
    }

Now, in the main() function, we need to register this callback by subscribing:

    msg_t_subscribe(zcm, "HELLO_WORLD", callback_handler, NULL);

Finally, in order to dispatch messages to the callbacks, ZCM needs a thread. So, we
call into zcm to tell it to consume the current thread and use it for dispatching any
incoming messages (zcm\_run doesn't normally return):

    zcm_run(zcm);

Alternatively you can call `zcm_start(zcm);` to spawn a new thread that calls `zcm_run(zcm);`

And that's it! Here's the full program (subscribe.c):

    #include <stdio.h>
    #include <zcm/zcm.h>
    #include <msg_t.h>

    void callback_handler(const zcm_recv_buf_t *rbuf, const char *channel, const msg_t *msg, void *usr)
    {
        printf("Received a message on channel '%s'\n", channel);
        printf("msg->str = '%s'\n", msg->str);
        printf("\n");
    }

    int main(int argc, char *argv[])
    {
        zcm_t *zcm = zcm_create("ipc");
        msg_t_subscribe(zcm, "HELLO_WORLD", callback_handler, NULL);

        zcm_run(zcm);

        // Can also call zcm_start(zcm); to spawn a new thread that calls zcm_run(zcm);
        //
        // zcm_start(zcm)
        // while(!done) { do stuff; sleep; }
        // zcm_stop(zcm);
        //

        zcm_destroy(zcm);
        return 0;
    }

Building and running:

    cc -o subscribe -I. subscribe.c msg_t.c -lzcm
    ./subscribe

Now, with both `./publish` and `./subscribe` running, you should successfully
see a stream of data in the subscribe window!

    Received a message on channel 'HELLO_WORLD'
    msg->str = 'Hello, World!'

## Type Safety

Another super useful feature of the ZCM type system is its type-checking capabilities.
These type checks make it hard to accidentally change message types without updating
programs that rely on them. For a mission critical system such as a robotics system, this
is a crucial feature.

Let's explore by example. Let's revisit our msg\_t type and add a new field, changing
it to look as follows:

    struct msg_t
    {
        boolean can_frobinate;
        string str;
    }

Adding this new field has changed the type's encoding and any existing program not aware
of the change might decode the wrong bytes! Let's regenerate the zcmtypes:

    zcm-gen -c msg_t.zcm && cat msg_t.h

The `msg_t.h` file now contains:

    /* ... code removed ... */
    typedef struct _msg_t msg_t;
    struct _msg_t
    {
        int8_t     can_frobinate;
        char*      str;
    };
    /* ... code removed ... */

Evolving a message in this way would normally eliminate any chance at backward
compatibility, but these things can happen accidentally and can introduce bugs
that are tricky to track-down. To show how ZCM detects this issue. Let's change
publish.c to use the new type:

    msg.can_frobinate = 0;

If we rebuild the publisher and try running it with the old subscriber, the subscriber
reports:

    error -1 decoding msg_t!!!

The decoder was able to successfully determine a mismatch, and correctly rejected the
incoming data. How does this work? Well, internally ZCM computes a hash code for each
type based on the individual field types and their orderings. If any of these types
change or get re-ordered, existing programs will detect a hashcode mismatch and will
refuse to decode the received data!


## Advanced ZCM Types

With a firm grasp of the ZCM tools and environment, we can start exploring
some of the more advanced topics in ZCM. Let's begin with the more
advanced features of the ZCM type system.

### Nested Types
As discussed earlier, ZCM types are strongly-typed statically-defined records
that may contain any number of fields. These fields can use a wide set of
primitive data types. However, this system is much more powerful than
containing only primitive types. The ZCM type system can support nested types
as well.

Let's see an example (nested\_types.zcm):

    struct position_t
    {
        double x, y, z;
    }

    struct cfg_t
    {
        position_t cam1;
        position_t laser1;
    }

Generate the bindings:

    zcm-gen -c nested_types.zcm

This generates the types `position_t` and `cfg_t`.

position\_t.h:

    /* ... code removed ... */
    typedef struct _position_t position_t;
    struct _position_t
    {
        double     x;
        double     y;
        double     z;
    };
    /* ... code removed ... */

cfg.h:

    /* ... code removed ... */
    typedef struct _cfg_t cfg_t;
    struct _cfg_t
    {
        position_t cam1;
        position_t laser1;
    };
    /* ... code removed ... */

As you can see, cfg\_t can be used in the same same way as any other zcmtype.
The type nesting *just works*!

### Array Types

As with any programming language type system, array data types are
often desired in a message. To this end, ZCM supports two different
kinds of array types: static and dynamic sized. As you'd expect, statically
size array use the same familiar C-style syntax:

    struct st_array_t
    {
        int32_t data[8];
    }

Dynamic arrays are similar with an additional size field:

    struct dyn_array_t
    {
        int64_t sz;
        int32_t data[sz];
    }

For dynamic arrays, the size parameter may be any integer type: `int8_t`,
`int16_t`, `int32_t`, or `int64_t`

Consistent with ZCM's philosophy, these types generate language bindings that
use the language-specific idioms:

C code:

    /* ... code removed ... */
    typedef struct _st_array_t st_array_t;
    struct _st_array_t
    {
        int32_t    data[8];
    };
    /* ... code removed ... */

    /* ... code removed ... */
    typedef struct _dyn_array_t dyn_array_t;
    struct _dyn_array_t
    {
        int64_t    sz;
        int32_t    *data;
    };
    /* ... code removed ... */

C++ code:

    class st_array_t
    {
        public:
            int32_t    data[8];

        /* ... code removed ... */
    };

    class dyn_array_t
    {
        public:
            int64_t    sz;

            std::vector< int32_t > data;

        /* ... code removed ... */
    };

Note that the size parameter must be declared before the array type, otherwise zcm
wont yet know how big of an array it should be decoding when it goes to decode it!

### Multi-dimmension Arrays

ZCM also supports multiple dimensions on arrays. You can also mix
static-sized and dynamic-sized arrays along different dimensions:

    struct multdim_t
    {
        int32_t dim1;
        int32_t dim2;
        int32_t dim3;

        float array1[dim1][8][dim3];
        float array2[2][dim2][5];
    }

### Constants

ZCM supports consts embedded in the type. These constants occupy no
space in the resulting serialization: they are only present in the
language-specific generated code. Common uses for consts are for
emulating enum types and for defining flags and masks:

    struct consts_t
    {
        const int32_t VAL_FOO = 0;
        const int32_t VAL_BAR = 1;
        const int32_t VAL_BAZ = 2;
        int32_t val;

        const int32_t FLAG_A = 1;
        const int32_t FLAG_B = 2;
        const int32_t FLAG_C = 4;
        const int32_t MASK_AB = 0x03;
        int32_t flags;

        const float secs_per_tick = 0.01;
        float ticks;
    }

### Closing Thoughts on Types

The ZCM type system is incredibly rich, flexible, and composable. Nearly each
of these features can be used independent of or in conjunction with the others.
This allows users to create rich and interesting message types with ease. For a rigorous
definition of the ZCM type system, see the [ZCM Type System](zcmtypesys.md).

## Next Steps

At this point you should know enough about ZCM to implement many interesting
applications. The essence of ZCM defining ZCMTypes, coding to the pub/sub APIs,
and debugging using the built-in tool suite. This covers about 90% of the
typical ZCM development workflow.

But, there is much more to ZCM, so we recommend a few *next steps* below:

 - [Dependencies & Building](building.md)
 - [Transport Layer](transports.md)
 - [Embedded Applications](embedded.md).
 - [Tools](tools.md)
 - [Frequently Asked Questions](FAQs.md)
 - [Project Philosophy](philosophy.md)
 - [Contributing](contributing.md)

<hr>
<a style="margin-right: 1rem;" href="javascript:history.go(-1)">Back</a>
[Home](../README.md)
