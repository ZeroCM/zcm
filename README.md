Zero Communications and Marshalling (ZCM)

ZCM is a micro-framework for mesaage-passing and data-marshalling, designed originally
for robotics systems where high-bandwidth and low latency are critical and the variance in
compute platformes is large. ZCM provides a publish/subscribe message passing model and
automatic marshalling/unmarshalling code generation with bindings for applications in a
variety of programming languages. ZCM also reamins transport agnostic and defines only
a transport interface so that it can run on anything from a high-end linux compute cluster
to a low-end real-time embedded-system with no operating system.

ZCM is a derivation of the LCM project created in 2006 by the MIT DARPA Urban Challenge
team. The core message-type system, publish/subscribe APIs, and basic tools are ported
directly from LCM and remain about 99% compatible. While there are a handful of subtle
differences between the two, the core distinguishing feature is ZCM's transport
agnosticism. LCM is designed competely around UDP Multicast. This trasport makes a lot
of sense for LAN connected compute clusters (such the original 2006 MIT DGC Vechicle).
However, there are many other applications that are interesting targets for ZCM messaging.
These include, local system messaging (IPC), multi-threaded messaging (in-procees),
embedded-system periphials (UART, I2C, etc), web browser applications (Web Sockets).
By refusing to make hard assumptions about the transport layer, ZCM opens the door
to a wide set of use-cases that we never possible or practical with LCM.

# Quick Links

* [ZCM downloads]()
* [Website and documentation]()

# Features

* Type-safe message marshalling
* A useful suite of tools for logging, log-playback, real-time message inspection (spy)
* A wide set of built-in transports incl. UDP Multicast, IPC, In-Process, and Serial
* An easy-to-use and well-defined interface for building custom transports
* Strong support for embedded uses. The core embedded code is compliant C89.
* Only one true dependency: A reasonably modern C++11 compiler for the non-embedded code.
* Some of the transports are built-on top of ZeroMQ, but this dependency is opt-in.

## Supported platforms and languages

* Platforms:
  * GNU/Linux
  * Any C89 capable embedded system
* Languages
  * C89 or greater
  * C++
  * Java
  * MATLAB (using Java)
  * Python

## Roadmap

* Port LCM UDP Multicast to achieve full backwards compatibility

* Platform Support:
  * OS X
  * Windows
  * Any POSIX-1.2001 system (e.g., Cygwin, Solaris, BSD, etc.)

* Consider porting the rest of the LCM languages
  * C#
  * Lua

* Explore alternative messaging types using ZCM Types, such as those found in ZeroMQ

## Subtle differences to LCM

* TODO