#include "Common.hpp"
#include "GetOpt.hpp"
#include "ZCMGen.hpp"
#include "emit/Emit.hpp"

extern "C" {
#include "version.h"
#include "tokenize.h"
}

int main(int argc, char *argv[])
{
    GetOpt gopt;
    gopt.addBool('h',  "help",     0,    "Show this help");
    gopt.addBool('t',  "tokenize", 0,    "Show tokenization");
    gopt.addBool('d',  "debug",    0,    "Show parsed file");
    gopt.addBool(0,    "lazy",     0,    "Generate output file only if .zcm is newer");
    gopt.addString(0,    "package-prefix",     "",
                      "Add this package name as a prefix to the declared package");
    gopt.addBool(0,  "version",    0,    "Show version information and exit");

    // we only support portable declarations now.
    // getopt_add_bool  (gopt, 0,    "warn-unsafe", 1, "Warn about unportable declarations");

    gopt.addSpacer("**** C options ****");
    gopt.addBool('c', "c",         0,     "Emit C code");
    setupOptionsC(gopt);

    gopt.addSpacer("**** C++ options ****");
    gopt.addBool('x', "cpp",         0,     "Emit C++ code");
    setupOptionsCpp(gopt);

    gopt.addSpacer("**** Java options ****");
    gopt.addBool('j', "java",      0,     "Emit Java code");
    setupOptionsJava(gopt);

    gopt.addSpacer("**** Python options ****");
    gopt.addBool('p', "python",      0,     "Emit Python code");
    setupOptionsPython(gopt);

    // getopt_add_spacer(gopt, "**** Javascript options ****");
    // getopt_add_bool  (gopt, 's', "js",      0,     "Emit Javascript code");
    // setup_js_options(gopt);

    // getopt_add_spacer(gopt, "**** Lua options ****");
    // getopt_add_bool  (gopt, 'l', "lua",      0,     "Emit Lua code");
    // setup_lua_options(gopt);

    // getopt_add_spacer(gopt, "**** C#.NET options ****");
    // getopt_add_bool  (gopt, 0, "csharp",      0,     "Emit C#.NET code");
    // setup_csharp_options(gopt);

    if (!gopt.parse(argc, argv, 1) || gopt.getBool("help")) {
        printf("Usage: %s [options] <input files>\n\n", argv[0]);
        gopt.doUsage();
        return 0;
    }

    ZCMGen zcm;
    zcm.gopt = &gopt;

    for (auto& fname : gopt.extraargs)
        if (int res = zcm.handleFile(fname))
            return res;

    // If "--version" was specified, then show version information and exit.
    if (gopt.getBool("version")) {
      printf("zcm-gen %d.%d.%d\n", ZCM_MAJOR_VERSION, ZCM_MINOR_VERSION,
             ZCM_MICRO_VERSION);
      return 0;
    }

    // If "-t" or "--tokenize" was specified, then show tokenization
    // information and exit.
    if (gopt.getBool("tokenize")) {
        return 0;
    }

    int did_something = 0;
    if (gopt.getBool("debug")) {
        did_something = 1;
        zcm.dump();
    }

    if (gopt.getBool("c")) {
        did_something = 1;
        if (emitC(zcm)) {
            printf("An error occurred while emitting C code.\n");
        }
    }

    if (gopt.getBool("cpp")) {
        did_something = 1;
        if (emitCpp(zcm)) {
            printf("An error occurred while emitting C++ code.\n");
        }
    }

    if (gopt.getBool("java")) {
        did_something = 1;
        if (emitJava(zcm)) {
            printf("An error occurred while emitting Java code.\n");
        }
    }

    if (gopt.getBool("python")) {
        did_something = 1;
        if (emitPython(zcm)) {
            printf("An error occurred while emitting Python code.\n");
        }
    }

    // if (getopt_get_bool(gopt, "js")) {
    //     did_something = 1;
    //     if (emit_js(zcm)) {
    //         printf("An error occurred while emitting Javascript code.\n");
    //     }
    // }

    // if (getopt_get_bool(gopt, "lua")) {
    // 	did_something = 1;
    // 	if (emit_lua(zcm)) {
    // 		printf("An error occurred while emitting Lua code.\n");
    // 	}
    // }

    // if (getopt_get_bool(gopt, "csharp")) {
    //     did_something = 1;
    //     if (emit_csharp(zcm)) {
    //         printf("An error occurred while emitting C#.NET code.\n");
    //     }
    // }

    if (did_something == 0) {
        printf("No actions specified. Try --help.\n");
    }

    return 0;
}
