#pragma once
#include "Common.hpp"

struct TypeMetadata
{
    i64 hash;
    string name;
    const zcm_type_info_t *info;
};

class TypeDb
{
public:
    TypeDb(const string& paths, bool debug=false);

    const TypeMetadata *getByHash(i64 hash);
    const TypeMetadata *getByName(const string& name);

private:
    bool loadtypes(const string& libname, void *lib);

private:
    bool debug;
    std::unordered_map<i64, TypeMetadata> hashToType;
    std::unordered_map<string, i64>  nameToHash;
};
