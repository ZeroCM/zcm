<a style="margin-right: 1rem;" href="javascript:history.go(-1)">Back</a>
[Home](../README.md)
# Dependencies & Building

ZCM uses WAF as it's build system. While seemingly obscure, WAF improves upon and replaces
GNU Autotools. It provides full configure, build, and install commands. It also supports building
multiple languages and elegantly handles custom build tools like zcm-gen. In-depth knowledge of
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
 - Java JNI: used for the Java language bindings and tools implemented in Java
 - NodeJS and socket.io: used for client-side web applications

All of the optional dependencies can be enabled or disabled at build-time by using
`./waf configure`. By default, *configure* will attempt to find and enable as many
dependencies as possible. If you wish to disallow a particular feature, you can
disable it.

## Building

Building is very similar to the GNU Autotools style, but recast in a Waf light:

    ./waf configure && ./waf build && sudo ./waf install

The command above will build the minimal ZCM possible with almost no features
enabled. For beginners, we suggest building ZCM with all features enabled so
tutorials and intros will work flawlessly. For all features:

    ./waf configure --use-all && ./waf build && sudo ./waf install

Unless you're lucky, the above command will probably fail due to a missing *Optional*
dependency (as listed above). For Ubuntu users, we have provided a dependency installation
script using:

    ./scripts/install-deps.sh

On other systems you may need to use your specific package manager to obtain the needed
packages. It should be noted that JNI sometimes needs `$JAVA_HOME` to be manually set.

At this point you should be able to start exploring ZCM. Check out the [Tutorial](tutorial.md).

## Advanced Build Options

As mentioned earlier, we can pick-and-choice the dependencies and features we desire.
As an example, imagine that we only intend to use the IPC transport and we don't want
a dependency to Java. We could build ZCM as follows:

    ./waf configure --use-ipc --use-zmq && ./waf build && ./test.sh && sudo ./waf install

There are many more configuration options that can be passed to the configure script. To
browse them all, simply run:

    ./waf --help

<hr>
<a style="margin-right: 1rem;" href="javascript:history.go(-1)">Back</a>
[Home](../README.md)
