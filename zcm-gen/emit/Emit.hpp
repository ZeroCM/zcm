#pragma once

void setupOptionsC(GetOpt& gopt);
int emitC(ZCMGen& zcm);

void setupOptionsJava(GetOpt& gopt);
int emitJava(ZCMGen& zcm);

void setupOptionsPython(GetOpt& gopt);
int emitPython(ZCMGen& zcm);

/* void setup_js_options(getopt_t *gopt); */
/* int emit_js(zcmgen_t *zcm); */

/* void setup_lua_options(getopt_t *gopt); */
/* int emit_lua(zcmgen_t *zcm); */

/* void setup_csharp_options(getopt_t *gopt); */
/* int emit_csharp(zcmgen_t *zcm); */

/* void setup_cpp_options(getopt_t *gopt); */
/* int emit_cpp(zcmgen_t *zcm); */
