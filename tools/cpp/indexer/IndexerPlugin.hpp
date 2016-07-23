#pragma once

#include <string>

// Remember you must inherit from this class but implement your functions
// inside a c file. That way you can then compile a shared library of your
// plugins

namespace zcm {

class IndexerPlugin
{
  public:
    // Must define the following function inside of your plugin
    static IndexerPlugin* makeIndexerPlugin();
    virtual ~IndexerPlugin();

    virtual std::string name() const;
    virtual bool includeInIndex(std::string channel, int64_t hash) const;
    // return true if "a" should come before "b" in the sorted list
    virtual bool lessThan(int64_t hash, uint8_t* a, size_t aLen, uint8_t* b, size_t bLen) const;
};

}
