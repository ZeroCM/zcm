#include "IndexerPlugin.hpp"

#include <memory>
#include <cxxabi.h>
#include <typeinfo>

using namespace zcm;

static inline std::string demangle(std::string name)
{
    int status = -4; // some arbitrary value to eliminate the compiler warning

    std::unique_ptr<char, void(*)(void*)> res {
        abi::__cxa_demangle(name.c_str(), NULL, NULL, &status),
        std::free
    };

    return (status==0) ? res.get() : name ;
}

IndexerPlugin* IndexerPlugin::makeIndexerPlugin()
{ return new IndexerPlugin(); }

IndexerPlugin::~IndexerPlugin()
{}

std::string IndexerPlugin::name() const
{ return "timestamp"; }

std::vector<std::string> IndexerPlugin::dependencies() const
{ return {""}; }

// Index every message according to timestamp
std::vector<std::string> IndexerPlugin::includeInIndex(std::string channel,
                                                       std::string typeName,
                                                       int64_t hash,
                                                       const char* data,
                                                       int32_t datalen) const
{ return {channel, typeName}; }

// This will enforce the order in which they are given to this function
bool IndexerPlugin::lessThan(int64_t hash, const char* a, int32_t aLen,
                                           const char* b, int32_t bLen) const
{ return false; }