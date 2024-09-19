cc_library(
    name = "zcm",
    srcs = [
        "zcm/blocking.cpp",
        "zcm/eventlog.c",
        "zcm/json/jsoncpp.cpp",
        "zcm/nonblocking.c",
        "zcm/transport/generic_serial_circ_buff.c",
        "zcm/transport/generic_serial_transport.c",
        "zcm/transport/transport_can.cpp",
        "zcm/transport/transport_file.cpp",
        "zcm/transport/transport_inproc.cpp",
        "zcm/transport/transport_serial.cpp",
        "zcm/transport/transport_zmq_local.cpp",
        "zcm/transport/udp/buffers.cpp",
        "zcm/transport/udp/mempool.cpp",
        "zcm/transport/udp/udp.cpp",
        "zcm/transport/udp/udpsocket.cpp",
        "zcm/transport_registrar.c",
        "zcm/url.cpp",
        "zcm/zcm.c",
    ],
    hdrs = glob([
        "zcm/**/*.h",
        "zcm/**/*.hpp",
        "zcm/**/*.h",
    ]),
    defines = [
        "USING_PYTHON",
        "USING_ZMQ",
        "USING_TRANS_CAN",
        "USING_TRANS_IPC",
        # "USING_TRANS_IPCSHM",
        "USING_TRANS_SERIAL",
        "USING_TRANS_UDP",
    ],
    includes = [
        "zcm",
    ],
    linkopts = ["-lzmq"],
    visibility = ["//visibility:public"],
    deps = [":zcm-util"],
)

cc_library(
    name = "zcm-util",
    srcs = [
        "zcm/util/debug.cpp",
        "zcm/util/lockfile.cpp",
        "zcm/util/topology.cpp",
    ],
    hdrs = glob([
        "util/**/*.h",
        "util/**/*.hpp",
        "zcm/**/*.h",
        "zcm/**/*.hpp",
    ]),
    includes = [
        "./",
        "zcm",
    ],
    linkopts = ["-lzmq"],
)

cc_library(
    name = "zcm-gen-lib",
    srcs = glob(
        [
            "gen/*.c",
            "gen/*.cpp",
            "gen/**/*.c",
            "gen/**/*.cpp",
            "zcm/json/*.cpp",
        ],
        exclude = [
            "gen/Main.cpp",
            "gen/disabled/*",
        ],
    ),
    hdrs = glob(
        [
            "gen/*.h",
            "gen/*.hpp",
            "gen/**/*.h",
            "gen/**/*.hpp",
            "util/*.h",
            "util/*.hpp",
            "zcm/json/*.h",
        ],
    ),
    defines = [
        "ENABLE_MEMBERNAME_HASHING",
        "ENABLE_TYPENAME_HASHING",
    ],
    includes = [
        "gen",
        "util",
    ],
    visibility = ["//visibility:public"],
)

exports_files(["gen/Main.cpp"])
