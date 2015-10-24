<a href="javascript:history.go(-1)">Back</a>
# Dependencies & Building

ZCM uses WAF as it's build system. While seemingly obscure, WAF improves upon and replaces
GNU Autotools. It provides full configure, build, and install commands. It also supports building
multiple lanaguages and elegantly handles custom build tools like zcm-gen. In-depth knowledge of
WAF scripting is not required for most tasks, but we refer users to the
[Waf Book](https://waf.io/book) as a reference.

## Dependencies

ZCM is designed with minimal dependencies in mind. Nearly all dependencies are opt-in.
The required dependencies are very reasonable and often available by default in many
modern linux systems.

### Required

 -  Bash: used for shell scripts
 -  Python (version >= 2.5): used for build scripting
 -  C++11 compiler: zcm-gen and other non-embedded core libraries are written in C++11

### Optional

 - All built-in transports: inclusion can be disabled at build-time
 - ZeroMQ: used for the `ipc` and `inproc` transports
 - Java JNI: used for the Java lanaguage bindings and tools implemented in Java
 - NodeJS and socket.io: used for client-side web applications

All of the optional dependencies can be enabled or disabled at build-time by using
`./waf configure`. By default, *configure* will attempt to find and enable as many
dependencies as possible. If you wish to disallow a particular feature, you can
disable it.

## Configuring

The configure step allows tweaking of the build and the dependencies that are to
be used. For this, run:

    ./waf configure

This will check for all dependencies and set-up the build environment. If you wish
to use different dependencies or settings, you can re-run with some command-line options.
For a list of the available options:

    ./waf --help

Configure is the step most likely to fail in the build sequence. Typically configure
fails due to missing dependecies.

## Building

After sucessful configuring, building is just:

    ./waf build

## Testing

You can verify a correct build by running the ZCM regresssion test suite:

    ./test.sh

## Installing

To use ZCM, we must install into the system directories:

    sudo ./waf install

## In One Command

In summary, we can do all of the above in one simple command:

    ./waf configure && ./waf build && ./test.sh && sudo ./waf install

<hr>
<a href="javascript:history.go(-1)">Back</a>
