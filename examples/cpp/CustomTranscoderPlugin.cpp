#include <vector>
#include <cassert>
#include <iostream>
#include <string>
#include <algorithm>

#include <zcm/tools/TranscoderPlugin.hpp>

#include "types/example_t.hpp"
#include "types/example2_t.hpp"

// REMEMBER: Do not define any functions where they are declared in the class
// See TranscoderPlugin.hpp for further explanation
class CustomTranscoderPlugin : public zcm::TranscoderPlugin
{
  private:
    zcm::LogEvent newEvt = {};
    uint8_t* newData;

  public:
    static zcm::TranscoderPlugin* makeTranscoderPlugin();

    CustomTranscoderPlugin();
    virtual ~CustomTranscoderPlugin();

    std::vector<const zcm::LogEvent*>
        transcodeEvent(int64_t hash, const zcm::LogEvent* evt) override;
};


zcm::TranscoderPlugin* CustomTranscoderPlugin::makeTranscoderPlugin()
{ return new CustomTranscoderPlugin(); }

CustomTranscoderPlugin::CustomTranscoderPlugin() : newData(nullptr) {}

CustomTranscoderPlugin::~CustomTranscoderPlugin()
{}

std::vector<const zcm::LogEvent*>
CustomTranscoderPlugin::transcodeEvent(int64_t hash, const zcm::LogEvent* evt)
{
    if (hash != example_t::getHash())
        return TranscoderPlugin::transcodeEvent(hash, evt);

    example_t e;
    e.decode(evt->data, 0, evt->datalen);

    example2_t e2 {};

    e2.timestamp2 = e.timestamp;

    e2.position2[0] = e.position[0];
    e2.position2[1] = e.position[1];
    e2.position2[2] = e.position[2];

    e2.orientation2[0] = e.orientation[0];
    e2.orientation2[1] = e.orientation[1];
    e2.orientation2[2] = e.orientation[2];
    e2.orientation2[3] = e.orientation[3];

    e2.num_ranges2 = e.num_ranges;
    e2.ranges2.resize(e2.num_ranges2);
    for (int i = 0; i < e2.num_ranges2; ++i)
        e2.ranges2[i] = e.ranges[i];

    e2.name2 = e.name;

    e2.enabled2 = e.enabled;

    int size = e2.getEncodedSize();
    delete newData;
    newData = new uint8_t[size];
    size = e2.encode(newData, 0, size);

    newEvt.timestamp = evt->timestamp;
    newEvt.channel = evt->channel;
    newEvt.data = newData;
    newEvt.datalen = size;

    return { &newEvt };
}
