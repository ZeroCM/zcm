<a style="margin-right: 1rem;" href="javascript:history.go(-1)">Back</a>
[Home](../README.md)
# Project Philosophy

## Problem

To explain ZCM, it's best to start with the problem it solves. Suppose you are developing a
distributed application. The application may be distributed over a wide-area network, a local LAN
network, or over a set of cores on a single machine. The distinction of granularity doesn't matter.
In all of these situations, one of the fundamental problems that arises is data transfer between the
various nodes in the system.

### Manual Approach

At first, we might not see this as a problem and opt to manually roll
data types and encodings. As an example, we may want to send an *orientation* to another process.
An *orientation* consists of a roll, pitch and yaw. We might decide to implement a send function as follows:

    struct orientation_t
    {
        float roll, pitch, yaw;
    };

    void send_rpy(const orientation_t *orient)
    {
        const size_t SZ = 3*sizeof(float);
        char buffer[SZ];
        encode_float(&buffer[0*sizeof(float)], orient->roll);
        encode_float(&buffer[1*sizeof(float)], orient->pitch);
        encode_float(&buffer[2*sizeof(float)], orient->yaw);
        send_bytes(buffer, SZ);
    }

The code assumes we have the following encoding functions:

    void encode_float(char *buf, float v)
    {
        encode_u32(mbuf, *(uint32_t*)&v);
    }

    void encode_u32(char *buf, uint32_t v)
    {
        // Need to encode in big-endian (network format)
        buf[0] = (v>>24)&0xff;
        buf[1] = (v>>16)&0xff;
        buf[2] = (v>>8)&0xff;
        buf[3] = (v>>0)&0xff;
    }

Wow, that is a lot of work for such a simple type! It also seems very tedious and error-prone. It's far too easy to
forget about network byte order or to forget the correct way to shift and mask integers.

To make matters worse, in practice the data looks much worse. Consider the following data structs:

    struct element_t
    {
        uint64_t timestamp;
        float roll, pitch, yaw;
        uint32_t count;
        uint16_t flags;
        string error_message;
    };

    struct message_t
    {
        int flags;
        size_t sz;
        element_t array[sz];
    }

Consider the coding effort required for hand-rolled encode/decode routines of this data.
Sure, we could write them, and for a veteran coder, it might not even take that long.
But, imagine we have countless messages with this complexity. This quickly will become
a complexity management issue.

However, there are many more disconcerting problems. Imagine a developer discovers that
the roll, pitch, and yaw fields above need more precision and decides an upgrade to
double precision is required. How should she approach this issue? What about migrating
all of the existing distributed nodes to the new layout? What happens if it's a very
large development team relying on the interface? At a minimum, how can we guarantee
that no user ever decodes data incorrectly?

### ASCII Approach

Things seems pretty dire. We need a better approach. How about an ASCII-based message format like XML or JSON?
For above structs, we could encode into JSON like this:

    {"flags":0,"sz":1,"array":[{"timestamp":123456789,"roll":1.234,"pitch":3.452,"yaw":32.345,"count":42,"flags":256,"error_message":"Error: Failed to Frobinate!"},{"timestamp":5126536,"roll":1.234,"pitch":3.452,"yaw":32.345,"count":67,"flags":16,"error_message":"Error: Couldn't Foo the Bar in its Baz"}]

Well, at least that seems easier. Is our problem solved? Well, yes and no.

#### The Good

This approach does solve some of the hairy binary format issues. Changing from a 16-bit to a 32-bit integer would work perfectly.
Also, the format is immune to a developer moving around struct fields since JSON object keys are unordered.

#### The Bad

Unfortunately, these benefits come from the fact that we don't really have any type system. The layout is enforced 100% by
convention. While this is convenient for small/simple uses, it gets torn apart by a growth in complexity.

#### The Ugly

Unfortunately there are many more issues with these types of formats:

  - We have to send the *schema* in the message
  - Encoding numbers in ascii is very inefficient
  - Encoding/Decoding between ascii data is very inefficient (versus binary)

But, the worst part of all is that we're still forced to write encode/decode functions
for each message type! We're still doing manual work and we're now also paying a runtime overhead!

### Dream Approach

So, let's step back and dream up a more ideal solution. Let's imagine if we could just define a complex
data type like `message_t` above and then send it like this:

    message_t msg;
    /* populate msg */
    send_message(m);

The `send_message()` function would automatically encode the message in an optimal binary format, and
send the encoded data through the desired transport protocol.

Then, we could imagine a receiver that simply gets a callback triggered:

    void handle_message(const message_t *msg)
    {
        /* do something with 'msg' */
    }

The callback dispatcher would have received the binary message data, automatically decoded it, and
sent the resulting message to any interested parties.

Since we're dreaming, imagine that each endpoint could verify in O(1) time that it *understands* the
type and data-layout of each message. The system would reject any invalid types and it would be impossible
to ever receive an invalid message due to the reasons listed above.

Let's go a step further: imagine that the programming-model could follow language-specific idioms and conventions.
All of the messy issues with data transfer, message encoding, and message verification could be hidden in the
send/recv layer. The application code could simply get on with it's main mission.

Then, we could trivially compose multiple languages. In C++, we'd code:

    struct Foo
    {
        std::string str;
        std::vector<double> nums;
    };

    void sendFoo()
    {
        Foo foo;
        foo.str = "This is my string";
        foo.nums = {1, 2, 3, 4, 5};
        send("FOO_CHANNEL", &foo);
    }

And then in Python, we'd code:

    class Foo(object):
        def __init__(self):
            self.str = ''
            self.nums = []

    def recvFoo(foo):
        print foo.str
        for v in foo.nums:
            print v

That would be pretty handy! But, have we traveled too far down the *rabbit-hole*?

## The Zen of ZCM

Luckily, for us dreamers, this depiction is precisely what ZCM aims to be! ZCM's primary value is its message type system.
ZCM, provides an extremely rich type system with support for nested/recursive types, array types, and enum/const values.
The message type-system is statically-typed and provides strong guarantees of message validity via hash codes computed based on
messages are encoded, sent, type-verified, and decoded into language-idiomatic bindings!

### The Tricky Bit

The tricky part with this *Dream Approach* is the transfer protocol. Until now, we've been very *hand-wavy* about it.
This is for a good reason; its hard! What should we use for the transfer protocol? On one hand, we could just stick with
TCP since it's fairly ubiquitous. But, then again, (1) we may not find a TCP/IP stack on every system, and (2) TCP is an awful
choice for some applications. For a real-time application on a LAN-connected cluster, we might quickly conclude that UDP
multicast is the perfect transport protocol, and we'd likely be right for this application. But, what about embedded
systems? How many of them actually have a reliable TCP/IP stack? Or, how about web applications where we only have ASCII-based
TCP/IP via WebSockets? In general, what transports can we reasonably expect a system to have?

### Transport Agnosticism

If you ponder this question long enough, you'll only emerge petrified. The only reasonable answer is unsatisfying: transports
are very platform-dependent. It is just not possible to appease every application that could benefit by using zcmtypes. **So,
ZCM doesn't chose a transport**. Instead ZCM treats transports as a platform/application specific issue, and provides
a lightweight microframework for implementing a ZCM-compliant transport. Out-of-the-box, ZCM provides a lot of conventional
transports, but non of them are strictly required. ZCM can work well on any system with any transport.

## The Mission

This is the essence of ZCM: *Statically Well-Defined Messages, Transport Agnosticism, and Language Agnosticism enabling
distributed applications using many heterogeneous compute platforms*. Whether it's an Embedded, Server, Cluster, or Web
distributed system, we aim to make it trivial to share and exchange common message types across language and system barriers.
We provide ZCM to make this easy.

## Next Steps

If you haven't already, we encourage you to check out our [Tutorial](tutorial.md).

Otherwise, you might like to learn about our community and how you can help make ZCM better: [Contributing](contributing.md).

<hr>
<a style="margin-right: 1rem;" href="javascript:history.go(-1)">Back</a>
[Home](../README.md)
