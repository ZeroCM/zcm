#pragma once

void setupOptionsC(GetOpt& gopt);
int emitC(ZCMGen& zcm);

void setupOptionsCpp(GetOpt& gopt);
int emitCpp(ZCMGen& zcm);

void setupOptionsJava(GetOpt& gopt);
int emitJava(ZCMGen& zcm);

void setupOptionsPython(GetOpt& gopt);
int emitPython(ZCMGen& zcm);

void setupOptionsNode(GetOpt& gopt);
int emitNode(ZCMGen& zcm);

void setupOptionsJulia(GetOpt& gopt);
int emitJulia(ZCMGen& zcm);
