#pragma once

#include <string>
#include <vector>

#include "zcm/zcm-cpp.hpp"
#include "zcm/json/json.h"

// Remember you must inherit from this class but implement your functions
// inside a c file. That way you can then compile a shared library of your
// plugins

// REPEAT: ALL FUNCTIONS MUST BE DEFINED OUTSIDE OF CLASS DECLARATION
//
// Do this:
//
// class CustomPlugin : IndexerPlugin
// {
//     static IndexerPlugin* makeIndexerPlugin();
// };
// IndexerPlugin* CustomPlugin::makeIndexerPlugin() { return new CustomPlugin(); }
//
//
// Not This:
//
// class CustomPlugin : IndexerPlugin
// {
//     static IndexerPlugin* makeIndexerPlugin() { return new CustomPlugin(); }
// };

namespace zcm {

class IndexerPlugin
{
  public:
    // Must define the following function inside of your plugin
    static IndexerPlugin* makeIndexerPlugin();
    virtual ~IndexerPlugin();

    virtual std::string name() const;
    virtual std::vector<std::string> dependencies() const;
    // Returns a vector of string indices indicating how the plugin would like
    // the data to be indexed
    // Indexer will use the returned vector in the following way:
    // index[IndexerPlugin::name()][retVec[0]][retVec[1]]...[retVec[n]]
    virtual std::vector<std::string> includeInIndex(std::string channel,
                                                    std::string typeName,
                                                    int64_t hash,
                                                    const char* data,
                                                    int32_t datalen) const;
    // return true if this plugin's lessThan function is valid
    virtual bool sorted() const;
    // return true if "a" should come before "b" in the sorted list
    virtual bool lessThan(off_t a, off_t b, zcm::LogFile& log,
                          const Json::Value& index) const;
};

}
