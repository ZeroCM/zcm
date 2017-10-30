#include <iostream>

#include "Common.hpp"
#include "Emitter.hpp"
#include "GetOpt.hpp"
#include "ZCMGen.hpp"

using namespace std;

void setupOptionsJulia(GetOpt& gopt)
{
    gopt.addString(0, "jupath", "", "Julia destination directory");
}

int emitJulia(ZCMGen& zcm)
{
    if (zcm.gopt->getBool("little-endian-encoding")) {
        printf("Julia does not currently support little endian encoding\n");
        return -1;
    }

    unordered_map<string, vector<ZCMStruct*> > packages;

    // group the structs by package
    for (auto& ls : zcm.structs)
        packages[ls.structname.package].push_back(&ls);

    for (auto& kv : packages) {
        auto& name = kv.first;
        cout << name << endl;
    }

    return 0;
}
