#include "util/TypeDb.hpp"

using namespace std;

namespace zcm {

bool TypeDb::findTypenames(vector<string>& result, const string& libname)
{
    result.clear();
    return true;
}

bool TypeDb::loadtypes(const string& libname, void* lib)
{
    return true;
}

TypeDb::TypeDb(const string& paths, bool debug) : debug(debug), isGood(true)
{
    (void)this->debug;
}

bool TypeDb::good() const
{
    return isGood;
}

const TypeMetadata* TypeDb::getByHash(int64_t hash) const
{
    return nullptr;
}

const TypeMetadata* TypeDb::getByName(const string& name) const
{
    return nullptr;
}

} // zcm
