<a style="margin-right: 1rem;" href="javascript:history.go(-1)">Back</a>
[Home](../README.md)
# From LCM to ZCM

This page is for introducing LCM users to the unique differences of ZCM. Here, we intend
to highlight strengths and weaknesses between the two, as well as tips and advice for
moving to ZCM

## Origin and Philosophy

ZCM grew out of frustration with LCM's strict union with the UDP Multicast transport.
While we were thrilled by the LCM message type system, we found the underlying transport
to be a constant headache in some applications we wished to develop.

In particular, we encountered:

  - Poor performance for inter-process message-passing
  - Poor support for embedded-system comms

We dreamed of being able to use LCM Types everywhere. Theoretically, we could use
the same type specification for messaging in countless contexts, including:

  - Inter-thread (shared-memory)
  - Inter-process (Unix Domain Sockets and shared-memory)
  - Inter-cluster (LCM-style UDPM)
  - Wide-Area Network Distributed System (TCP)
  - Embedded system interfaces (UART, I2C, USB, UDP, etc)
  - Client-side web-browser (WebSockets and Javascript)

For this, we created the ZCM project. Our goal is to connect all of these messaging
granularities with simple and easy-to-use APIs. We want to be able to define
a single ZCM message type and send and receive it anywhere.

## Core Differences

The ZCM project began by forking the LCM codebase and performing many strategic improvements:

  - Removing the dependency on GLib
  - Porting the core libraries to C++11

Our primary goal for v1.0 is to retain API compatibility with LCM, while providing a new
custom transport layer. Thus, despite being implemented in C++11, the ZCM C APIs are
identical to LCM. We have strived to retain as much API compatibility as possible for
our intial release.

## New Features with ZCM

### ZCM Transport Layer

ZCM has many benefits over LCM, as noted above, but its primary boon is the addition
of the new custom transport system. ZCM allows end-users to implement and register
custom transports. As long as a new transport conforms to the rigorously-defined
transport API, the custom transport will work flawlessly. To learn more, check out
the page on [Transport Layer](transports.md).

### ZCM Embedded-System Support

ZCM is designed with embedded applications in mind. The custom transport layer (see above),
has a special non-blocking transport API designed and fine-tuned for embedded uses. In addition,
the ZCM non-blocking core library is restricted to "on-the-metal" C89 code. This restriction allows
embedded systems to use ZCM types and ZCM non-blocking code with little or no modifications.
To learn more, check out the page on [Embedded Applications](embedded.md).

## Porting programs to ZCM

Currently, ZCM is approximately 95% API compatible with LCM. Porting existing Unix-based LCM
programs to ZCM is very easy in many cases. A quick `sed -i 's/lcm/zcm/g'` works for
most applications. ZCM uses the same binary-compatible formats for UDP Multicast, Logging,
and ZCMType encodings. Thus LCM and ZCM applications can communicate flawlessly. This
allows LCM users to gradually migrate to ZCM.

### Known incompatibilities:
 - `zcm_get_fileno()` is not supported
 - `zcm_subscription_set_queue_capacity` is not supported
 - LCMType-style enums are not supported
 - Language bindings not currently supported:
   - Lua
   - C#
 - Operating Systems not currently supported
   - Windows
   - OS X
   - Non-linux POSIX-1.2001 systems (e.g., Cygwin, Solaris, BSD, etc.)

### Other minor differences
 - The Java bindings now require JNI
 - The ZeroMQ library is currently required for the 'ipc' and 'inproc' transports

<hr>
<a style="margin-right: 1rem;" href="javascript:history.go(-1)">Back</a>
[Home](../README.md)
