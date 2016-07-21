#pragma once
#include "Common.hpp"

struct TypeMetadata
{
    i64 hash;
    std::string name;
    const zcm_type_info_t *info;
};

class TypeDb
{
public:
    TypeDb(const std::string& paths, bool debug=false);

    const TypeMetadata *getByHash(i64 hash);
    const TypeMetadata *getByName(const std::string& name);

private:
    bool findTypenames(std::vector<std::string>& result, const std::string& libname);
    bool loadtypes(const std::string& libname, void *lib);

private:
    bool debug;
    std::unordered_map<i64, TypeMetadata> hashToType;
    std::unordered_map<std::string, i64>  nameToHash;
};
