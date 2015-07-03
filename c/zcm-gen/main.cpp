#include "Common.hpp"
#include "zcmgen.hpp"
#include "version.h"

extern "C" {
#include "getopt.h"
#include "tokenize.h"
//#include <glib.h>
}

void setupOptionsC(getopt_t *gopt);
int emitC(ZCMGen& zcm);

/* void setup_java_options(getopt_t *gopt); */
/* int emit_java(zcmgen_t *zcm); */

/* void setup_python_options(getopt_t *gopt); */
/* int emit_python(zcmgen_t *zcm); */

/* void setup_js_options(getopt_t *gopt); */
/* int emit_js(zcmgen_t *zcm); */

/* void setup_lua_options(getopt_t *gopt); */
/* int emit_lua(zcmgen_t *zcm); */

/* void setup_csharp_options(getopt_t *gopt); */
/* int emit_csharp(zcmgen_t *zcm); */

/* void setup_cpp_options(getopt_t *gopt); */
/* int emit_cpp(zcmgen_t *zcm); */

int main(int argc, char *argv[])
{
    getopt_t *gopt = getopt_create();

    getopt_add_bool  (gopt, 'h',  "help",     0,    "Show this help");
    getopt_add_bool  (gopt, 't',  "tokenize", 0,    "Show tokenization");
    getopt_add_bool  (gopt, 'd',  "debug",    0,    "Show parsed file");
    getopt_add_bool  (gopt, 0,    "lazy",     0,    "Generate output file only if .zcm is newer");
    getopt_add_string(gopt, 0,    "package-prefix",     "",
                      "Add this package name as a prefix to the declared package");
    getopt_add_bool  (gopt, 0,  "version",    0,    "Show version information and exit");

    // we only support portable declarations now.
    // getopt_add_bool  (gopt, 0,    "warn-unsafe", 1, "Warn about unportable declarations");

    getopt_add_spacer(gopt, "**** C options ****");
    getopt_add_bool  (gopt, 'c', "c",         0,     "Emit C code");
    setupOptionsC(gopt);

    // getopt_add_spacer(gopt, "**** C++ options ****");
    // getopt_add_bool  (gopt, 'x', "cpp",         0,     "Emit C++ code");
    // setup_cpp_options(gopt);

    // getopt_add_spacer(gopt, "**** Java options ****");
    // getopt_add_bool  (gopt, 'j', "java",      0,     "Emit Java code");
    // setup_java_options(gopt);

    // getopt_add_spacer(gopt, "**** Python options ****");
    // getopt_add_bool  (gopt, 'p', "python",      0,     "Emit Python code");
    // setup_python_options(gopt);

    // getopt_add_spacer(gopt, "**** Javascript options ****");
    // getopt_add_bool  (gopt, 's', "js",      0,     "Emit Javascript code");
    // setup_js_options(gopt);

    // getopt_add_spacer(gopt, "**** Lua options ****");
    // getopt_add_bool  (gopt, 'l', "lua",      0,     "Emit Lua code");
    // setup_lua_options(gopt);

    // getopt_add_spacer(gopt, "**** C#.NET options ****");
    // getopt_add_bool  (gopt, 0, "csharp",      0,     "Emit C#.NET code");
    // setup_csharp_options(gopt);

    if (!getopt_parse(gopt, argc, argv, 1) || getopt_get_bool(gopt,"help")) {
        printf("Usage: %s [options] <input files>\n\n", argv[0]);
        getopt_do_usage(gopt);
        return 0;
    }

    ZCMGen zcm;
    zcm.gopt = gopt;

    // for (unsigned int i = 0; i < g_ptr_array_size(gopt->extraargs); i++) {
    //     char *path = (char *) g_ptr_array_index(gopt->extraargs, i);

    //     int res = zcmgen_handle_file(zcm, path);
    //     if (res)
    //         return res;
    // }

    // If "--version" was specified, then show version information and exit.
    if (getopt_get_bool(gopt, "version")) {
      printf("zcm-gen %d.%d.%d\n", ZCM_MAJOR_VERSION, ZCM_MINOR_VERSION,
             ZCM_MICRO_VERSION);
      return 0;
    }

    // If "-t" or "--tokenize" was specified, then show tokenization
    // information and exit.
    if (getopt_get_bool(gopt, "tokenize")) {
        return 0;
    }

    int did_something = 0;
    if (getopt_get_bool(gopt, "debug")) {
        did_something = 1;
        zcm.dump();
    }

    if (getopt_get_bool(gopt, "c")) {
        did_something = 1;
        if (emitC(zcm)) {
            printf("An error occurred while emitting C code.\n");
        }
    }

    // if (getopt_get_bool(gopt, "cpp")) {
    //     did_something = 1;
    //     if (emit_cpp(zcm)) {
    //         printf("An error occurred while emitting C++ code.\n");
    //     }
    // }

    // if (getopt_get_bool(gopt, "java")) {
    //     did_something = 1;
    //     if (emit_java(zcm)) {
    //         perror("An error occurred while emitting Java code.\n");
    //     }
    // }

    // if (getopt_get_bool(gopt, "python")) {
    //     did_something = 1;
    //     if (emit_python(zcm)) {
    //         printf("An error occurred while emitting Python code.\n");
    //     }
    // }

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
