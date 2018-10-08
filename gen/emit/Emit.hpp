#pragma once

#include "Common.hpp"
#include "GetOpt.hpp"

void setupOptionsC(GetOpt& gopt);
int emitC(const ZCMGen& zcm);
vector<string> getFilepathsC(const ZCMGen& zcm);

void setupOptionsCpp(GetOpt& gopt);
int emitCpp(const ZCMGen& zcm);
vector<string> getFilepathsCpp(const ZCMGen& zcm);

void setupOptionsJava(GetOpt& gopt);
int emitJava(const ZCMGen& zcm);
vector<string> getFilepathsJava(const ZCMGen& zcm);

void setupOptionsPython(GetOpt& gopt);
int emitPython(const ZCMGen& zcm);
vector<string> getFilepathsPython(const ZCMGen& zcm);

void setupOptionsNode(GetOpt& gopt);
int emitNode(const ZCMGen& zcm);
vector<string> getFilepathsNode(const ZCMGen& zcm);

void setupOptionsJulia(GetOpt& gopt);
int emitJulia(const ZCMGen& zcm);
vector<string> getFilepathsJulia(const ZCMGen& zcm);
