#pragma once

#include <string>

#include "zcm/IndexerPlugin.hpp"

struct IndexerPluginMetadata
{
    std::string className;
    zcm::IndexerPlugin* (*makeIndexerPlugin)(void);
    inline bool operator==(const IndexerPluginMetadata& o) { return className == o.className; }
};

class IndexerPluginDb
{
  public:
    // RRR: should probably put some documentation on what paths is supposed to look like in
    //      order to get the desired separation (looks like you are splitting on ":"). Why not
    //      just pass in a vector of strings here (and do the split in the main() if that's
    //      really how you want to feed it command line args
    IndexerPluginDb(const std::string& paths, bool debug = false);
    ~IndexerPluginDb();
    std::vector<const zcm::IndexerPlugin*> getPlugins();

  private:
    bool findPlugins(const std::string& libname);
    bool debug;
    std::vector<IndexerPluginMetadata> pluginMeta;
    // RRR: for the non-const one, you can use unique ptrs (makes for cleaner cleanup)
    std::vector<zcm::IndexerPlugin*> plugins;
    std::vector<const zcm::IndexerPlugin*> constPlugins;
};
