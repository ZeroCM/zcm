#pragma once

#include <string>
#include <cstdint>

#include "zcm/zcm-cpp.hpp"

//
// Remember you must inherit from this class and implement your functions
// inside your cpp file. That way you can then compile a shared library of your
// plugins.
//
// REPEAT: ALL FUNCTIONS MUST BE *DEFINED* OUTSIDE OF CLASS *DECLARATION*
//
// Do this:
//
// class CustomPlugin : TranscoderPlugin
// {
//     static TranscoderPlugin* makeTranscoderPlugin();
// };
// TranscoderPlugin* CustomPlugin::makeTranscoderPlugin() { return new CustomPlugin(); }
//
//
// Not This:
//
// class CustomPlugin : TranscoderPlugin
// {
//     static TranscoderPlugin* makeTranscoderPlugin() { return new CustomPlugin(); }
// };
//
//
// Note that an hpp file is not even required. You only need a cpp file containing
// your custom plugin. Then compile it into a shared library by doing the following:
//
// g++ -std=c++11 -fPIC -shared CustomPlugin.cpp -o plugins.so
//

namespace zcm {

class TranscoderPlugin
{
  public:
    // Must declare the following function for your plugin. Unable to enforce
    // declaration of static functions in inheritance, so this API trusts you
    // to define this function for your type
    static TranscoderPlugin* makeTranscoderPlugin() { return new TranscoderPlugin(); }

    virtual ~TranscoderPlugin() {}

    //
    // hash is the hash of the type encoded inside the event
    //
    //  if (hash == msg_t::getHash()) {
    //      old_t oldMsg;
    //      oldMsg.decode(evt->data, 0, evt->datalen);
    //  }
    //  new_t newMsg;
    //  ...
    //  convert(newMsg, oldMsg);
    //  ...
    //  int size = newMsg.getEncodedSize();
    //  delete newData; // this variable would be a class variable
    //  newData = new uint8_t[size];
    //  size = newMsg.encode(newData, 0, size);
    //
    //  RRR (Tom) I think returning new memory for a LogEvent, which would then
    //  be cleaned up by the transcoder main would be more natural, but I could
    //  be convinced otherwise.
    //  RRR (Bendes) Then you can't `return {evt};` you'd have to copy it
    //
    //  // newEvt would be a class variable
    //  newEvt.timestamp = evt->timestamp;
    //  newEvt.channel = evt->channel;
    //  delete newEvt.data;
    //  newEvt.data = newData;
    //  newEvt.datalen = size;
    //
    //  return { newEvt };
    //
    //  return a vector of events you want to replace this event with.
    //  return TranscoderPlugin::transcodeEvent(hash,evt) to not transcode
    //  this event into the output log
    //
    virtual std::vector<const LogEvent*> transcodeEvent(int64_t hash, const LogEvent* evt)
    {
        return {};
    }
};

}
