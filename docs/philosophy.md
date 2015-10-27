<a href="javascript:history.go(-1)">Back</a>
# Project Philosophy

## Problem

To explain ZCM, it's best to start with the problem it solves. Suppose you are developing a
distributed application. The application may be distributed over a wide-area network, a local LAN
network, or over a set of cores on a single machine. The distinction of granularity doesn't matter.
In all of these situations, one of the fundamental problems that arises is data transfer between the
various nodes in the system.

### Manual Approach

At first, we might not see this as a problem and opt to manually-roll
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

Wow, that is a lot of work for such a simple type! To make matters worse, in practice the
data looks much worse. Consider the following data structs:

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

### Conventional Approach

We need a better approach. How about something like XML or JSON? For the above, we have:

    {"flags":0,"sz":1,"array":[{"timestamp":123456789,"roll":1.234,"pitch":3.452,"yaw":32.345,"count":42,"flags":256,"error_message":"Error: Failed to Frobinate!"}

From the start, this isn't much better:

  1. It requires much more space for the same data (at least 4X)
    - This translates directly to a bandwidth hit on the network
  2. Encoding/Decoding is much more expensive since we must convert between ASCII and Binary
  3. Implementing Encoding/Decoding functions is not really simpler
    - We must still manually iterate through each field to construct the JSON data
    - A custom codec is still required for each type

ATTACK JSON ON LACK OF MESSAGE CONTEXT/TYPE


    ### Give a crazy example of JSON image_t here
    # This will demonstrate the weekness of JSON for serious applications with large messages

### Dream Approach

This problem is precisely what ZCM aims to solve. In particular, imagine if we could just define a complex
data type like `message_t` above and then send it like this:

    message_t msg;
    /* populate msg */
    send_message(m);

The `send_message()` funtion would automatically encode the message in an optimal binary format, and
send the encoded data through the desired transport protocol.

Then, we could imagine a receiver that simply gets a callback triggered:

    void handle_message(const message_t *msg)
    {
        /* do something with 'msg' */
    }

The callback dispatcher would have received the binary message data, automatically decoded it, and
sent the resulting message to any interested parties.

In addition, the programming-model could follow language-specific idioms and conventions. All of the messy issues
with data transfer, message encoding, and message verification could be hidden in the send/recv layer. The application
code could simply get on with it's main mission.

## ZCM as a Solution

In a nutshell, the *Dream Approach* is precisely what ZCM aims to provide. ZCM's primary value is its message type system.
ZCM, provides an extremely rich type system with support for nested/recursive types, array types, and enum/const values.
The message type-system is statically-typed and provides strong guarentees of message validity. If a message type is changed,
ZCM will detect the changes, and reject all invalid messsges. It is simply not possible to incorrectly decode binary data with
ZCM.

The tricky bit with the *Dream Approach* listed above is the transfer protocol. What should be done with the transfer
protocol. On one hand, TCP is fairly ubiquitous. On the other, (1) its not present on every system and (2) it's an awful
choice for some applications. For a real-time application on a LAN-connected cluster, we might quickly conclude that UDP
multcast is the perfect transport protocal, and we'd be right for this application, but not others. What about embedded
systems? How many of them actually have a usable TCP/IP stack? How about web applications where we only have ASCII-based
TCP/IP? What transports can we expect systems to have?

The transport question is complicated and appears to be very platform-dependent. For this reason, ZCM treats transports
as a platform/application specific issue. ZCM refuses to take any side on transports and stays 100% transport agnostic.
Instead, ZCM provides a very solid mesaage type system and a well-defined transport interface. This approach allows
ZCM to be useful on any type of platform.

## The Essence

Whether it is Embedded, Server, Cluster, or Web, distrubuted systems can share common message types and communicate with
each-other. This is the essence of ZCM: Statically Well-Defined Messages and Transport Agnosticism enabling distubuted
applications using many heterogeneous compute platforms.

## Next Steps

If you haven't already, we encourage you to check out our [Tutorial](tutorial.md).

Otherwise, you might like to learn about our community: [Contributing](contributing.md).

<hr>
<a href="javascript:history.go(-1)">Back</a>
