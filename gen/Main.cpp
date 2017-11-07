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
    gopt.addBool(0,    "package",  0,    "Show only package");
    gopt.addBool('d',  "debug",    0,    "Show parsed file");
    gopt.addBool(0,    "lazy",     0,    "Generate output file only if .zcm is newer");
    gopt.addString(0,    "package-prefix",     "",
                      "Add this package name as a prefix to the declared package");
    gopt.addBool(0,  "little-endian-encoding", 0, "Encode and decode network traffic in little endian format");
    gopt.addBool(0,  "version",    0,    "Show version information and exit");

    gopt.addSpacer("**** C options ****");
    gopt.addBool('c', "c",         0,     "Emit C code");
    setupOptionsC(gopt);

    gopt.addSpacer("**** C++ options ****");
    gopt.addBool('x', "cpp",       0,     "Emit C++ code");
    setupOptionsCpp(gopt);

    gopt.addSpacer("**** Java options ****");
    gopt.addBool('j', "java",      0,     "Emit Java code");
    setupOptionsJava(gopt);

    gopt.addSpacer("**** Python options ****");
    gopt.addBool('p', "python",    0,     "Emit Python code");
    setupOptionsPython(gopt);

    gopt.addSpacer("**** Node.js options ****");
    gopt.addBool('n', "node",      0,     "Emit Node.js code");
    setupOptionsNode(gopt);

    bool parseSuccess = gopt.parse(argc, argv, 1);
    if (!parseSuccess || gopt.getBool("help")) {
        printf("Usage: %s [options] <input files>\n\n", argv[0]);
        gopt.doUsage();
        return !parseSuccess;
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

    // If "-p" or "--package" was specified, then print out package and exit
    if (gopt.getBool("package")) {
        string package = "";
        for (auto& s : zcm.structs) {
            if (package != "" && s.structname.package != package) {
                fprintf(stderr, "Multiple types with different packages specified.\n");
                return 1;
            }
        }
        printf("%s\n", package.c_str());
        return 0;
    }

    int did_something = 0;
    int ret = 0;
    if (gopt.getBool("debug")) {
        did_something = 1;
        zcm.dump();
    }

    if (gopt.getBool("c")) {
        did_something = 1;
        if (emitC(zcm)) {
            printf("An error occurred while emitting C code.\n");
            ret = 1;
        }
    }

    if (gopt.getBool("cpp")) {
        did_something = 1;
        if (emitCpp(zcm)) {
            printf("An error occurred while emitting C++ code.\n");
            ret = 1;
        }
    }

    if (gopt.getBool("java")) {
        did_something = 1;
        if (emitJava(zcm)) {
            printf("An error occurred while emitting Java code.\n");
            ret = 1;
        }
    }

     if (gopt.getBool("python")) {
         did_something = 1;
         if (emitPython(zcm)) {
             printf("An error occurred while emitting Python code.\n");
             ret = 1;
         }
     }

    if (gopt.getBool("node")) {
        did_something = 1;
        if (emitNode(zcm)) {
            printf("An error occurred while emitting Node.js code.\n");
            ret = 1;
        }
    }

    if (did_something == 0) {
        printf("No actions specified. Try --help.\n");
        ret = 1;
    }

    return ret;
}
