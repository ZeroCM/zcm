<a style="margin-right: 1rem;" href="javascript:history.go(-1)">Back</a>
[Home](../README.md)
# Embedded Applications

ZCM prides itself on strong support for embedded applications. This support allows
embedded systems to construct and send zcmtypes though a custom embedded transport to
other systems using ZCM.

The core nonblock library code follows the following principles:

  1. Standard-compliant C89 code
  2. Absolutely no dependencies (this includes any OS primitives)
  3. No blocking for any reason
  4. Designed for single-threaded use
  5. Minimal malloc() use. Currently, on startup only.

## Platform Requirements

While we strive to support many systems, there are a few requirements:

  - byte-addressable memory
  - 8-bit, 16-bit, 32-bit and 64-bit signed and unsigned integer types
     - Some may be emulated, but the standard C integer types must be available
  - a basic C standard library
  - malloc implementation

## Adding ZCM to a project

By default, the ZCM linux build gathers together the embedded-specific sources, checks them
for C89 compliance, and creates `build/zcm/zcm-embed.tar.gz`. Using ZCM is as simple as untaring
into your project's source tree. We recommend using the `zcm` subdir for all zcm library code.

For the generation of zcmtypes, you will need a standard linux ZCM install. You can simply use
`zcm-gen` and copy the resulting files into your project's source tree.

With some scripting, most embedded environments can be configured to use `zcm-gen` as a build
tool and all of the type generation can be made automatic.

## Using ZCM in code

In embedded ZCM there are no built-in transports. To use ZCM in an embedded application,
you must implement a nonblocking transport that uses your platform's hardware primatives.
In addition, the transport registrar and the url system is disabled. To create a `zcm_t`
object, you must use `zcm_create_from_trans()`. See the page on [Transport Layer](transports.md)
for details on implementing nonblocking transports. Note that the nonblocking api semantics
are different than the blocking ones which makes zcm more friendly to embedded applications.
Make sure you read the section on [Non-blocking API Semantics](transports.md) carefully.
Once implemented and constructed, you should be able to use ZCM the same way you would on
desktop systems! A generic serial transport is provided for you. An example of how to use it
is provided in the examples directory.

## Issues, Bugs, and Support

In embedded-land it's hard to guarantee that a library will work on any system. We care a lot
about our compatibility on embedded, but issues do happen. We always try to be proactive in
our handling of any issues that arise. If you find any violations with the principles and
requirements above, don't hesitate to reach out to the ZCM community. See
[Contributing](contributing.md) for details.

<hr>
<a style="margin-right: 1rem;" href="javascript:history.go(-1)">Back</a>
[Home](../README.md)
