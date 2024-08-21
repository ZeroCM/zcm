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

 - All built-in transports: inclusion must be enabled pre-compile-time
 - ZeroMQ: used for the `ipc` and `inproc` transports
 - Java JNI: used for the Java language bindings and tools implemented in Java
 - NodeJS and socket.io: used for client-side web applications. Note that Debian
   users should install the `nodejs-legacy` package in addition to the `nodejs`
   package because of the debian renaming of the "node" executable to "nodejs"
   will cause build problems.
 - Python: used for python language bindings
 - Julia: used for julia language bindings
 - elf: used by some tools for runtime loading of shared libraries of zcmtypes
 - clang: development tool used for testing zcm with clang's addresss and thread sanitizers
 - cxxtest: development tool used for verifying zcm core code

All of the optional dependencies must be enabled at build-time by using
`./waf configure`. By default, *configure* will not attempt to enable any
dependencies. If you wish to enable a particular feature, you can enable it with
a `--use` flag in the `./waf configure` step.

## Building

Building is very similar to the GNU Autotools style, but recast in a Waf light:

    ./waf configure
    ./waf build
    sudo ./waf install

The command above will build the minimal ZCM possible with almost no features
enabled. For beginners, we suggest building ZCM with all features enabled so
tutorials and intros will work flawlessly. For all features:

    ./waf configure --use-all
    ./waf build
    sudo ./waf install

Unless you're lucky, the above command will probably fail due to a missing *Optional*
dependency (as listed above). For Ubuntu users, we have provided a dependency installation
script using:

    ./scripts/install-deps.sh

On other systems you may need to use your specific package manager to obtain the needed
packages. It should be noted that JNI sometimes needs `$JAVA_HOME` to be manually set.
If you're still having issues building, check out our [FAQs](FAQs.md) for more info.
If you still can't find the answer to your question, feel free to
[reach out](https://discord.gg/T6jYM3eMjw)!

At this point you should be able to start exploring ZCM. Check out the [Tutorial](tutorial.md).

## Examples

Example source code is provided within the zcm code base.
Since they try to touch on as many usage cases of ZCM as possible,
building them requires the installed ZCM libraries to have been configured with
`--use-all`, as done above. Before the examples can be built, some environmental
variables needs to be set.
The easiest way to set these variables is to source the examples environment file:

    source ./examples/env

Now use Waf as before to build the examples

    ./waf configure
    ./waf build_examples

You can now run the examples located inside the build folder.
Try to run this inside one shell

    ./build/examples/examples/cpp/sub

And in another shell

    ./build/examples/examples/cpp/pub

The first shell should now start to print out the messages received.
For a guide on how to use the ZCM tooling to monitor, log and playback the messages
checkout the [Tools guide](tools.md)

## Advanced Build Options

As mentioned earlier, we can pick-and-choice the dependencies and features we desire.
As an example, imagine that we only intend to use the IPC transport and we don't want
a dependency to Java. We could build ZCM as follows:

    ./waf configure --use-ipc --use-zmq
    ./waf build
    sudo ./waf install

There are many more configuration options that can be passed to the configure script. To
browse them all, simply run:

    ./waf --help

Note that some of the zcm tools have dependencies that must be specified at
waf configure time. For a complete list of available tools and their dependencies,
take a look at the page on [Tools](tools.md).

For help with frequently asked questions, check out the *links* below:

 - [Frequently Asked Questions](FAQs.md)
 - [Project Philosophy](philosophy.md)
 - [Contributing](contributing.md)

<hr>
<a style="margin-right: 1rem;" href="javascript:history.go(-1)">Back</a>
[Home](../README.md)
