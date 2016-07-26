#pragma once

#include <string>
#include <vector>
#include <cstdint>

#include "zcm/zcm-cpp.hpp"
#include "zcm/json/json.h"

//
// Remember you must inherit from this class and implement your functions
// inside your cpp file. That way you can then compile a shared library of your
// plugins.
//
// REPEAT: ALL FUNCTIONS MUST BE *DEFINED* OUTSIDE OF CLASS *DECLARATION*
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
//
//
// Note that an hpp file is not even required. You only need a cpp file containing
// your custom plugin. Then compile it into a shared library by doing the following:
//
// g++ -std=c++11 -fPIC -shared CustomPlugin.cpp -o plugins.so
//

namespace zcm {

class IndexerPlugin
{
  public:
    // Must declare the following function for your plugin. Unable to enforce
    // declaration of static functions in inheritance, so this API trusts you
    // to define this function for your type
    static IndexerPlugin* makeIndexerPlugin();

    virtual ~IndexerPlugin();

    virtual std::string name() const;

    // Returns a vector of names of other plugins that this plugin depends on.
    // Ie if a custom plugin depends on the timestamp plugin's output, it would
    // return {"timestamp"} and no indexing functions would be called on this
    // plugin until the "timestamp" plugin finished its indexing
    // Specifying dependencies allows this plugin to use the timestamp-indexed
    // data while performing its own indexing operations
    virtual std::vector<std::string> dependsOn() const;

    // Do anything that your plugin requires doing before the indexing process
    // starts but after all dependencies have run
    virtual void setUp(const Json::Value& index,
                       Json::Value& pluginIndex,
                       zcm::LogFile& log);

    // pluginIndex is the index you should modify. It is your json object that
    // will be passed back to this function every time the function is called.
    //
    // index is the entire json object containing the output of every plugin run
    // so far
    //
    // channel is the channel name of the event
    //
    // typeName is the name of the zcmtype encoded inside the event
    //
    // offset is the index into the eventlog where this event is stored
    // each susbequent indexEvent call will be called with a larger offset
    // than the last. In other words, "offset" is monotonically increasing
    // across indexEvent calls
    //
    // timestamp is the timestamp of the event. Not to be confused with any
    // fields contained in the zcmtype message itself. This is the timestamp
    // of the event as reported by the transport that originally received it.
    //
    // hash is the hash of the type encoded inside the event
    //
    // data and datalen is the payload of the event
    // To recover the zcmtype message from these arguments, simply call:
    //
    //  if (hash == msg_t::getHash()) {
    //      msg_t msg;
    //      msg.decode(data, 0, datalen);
    //  }
    //
    // The default plugin indexes in the following way. You might want to
    // follow suit but that decision is left completely up to you.
    //
    // pluginIndex[channel][typeName].append(offset);
    //
    //
    virtual void indexEvent(const Json::Value& index,
                            Json::Value& pluginIndex,
                            std::string channel,
                            std::string typeName,
                            off_t offset,
                            uint64_t timestamp,
                            int64_t hash,
                            const char* data,
                            int32_t datalen);

    // Do anything that your plugin requires doing before the indexer exits
    // If your data needs to be sorted, do so here
    virtual void tearDown(const Json::Value& index,
                          Json::Value& pluginIndex,
                          zcm::LogFile& log);
};

}
