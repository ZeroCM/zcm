#include <cassert>
#include <iostream>

#include <zcm/IndexerPlugin.hpp>

#include "types/example_t.hpp"

class CustomIndexerPlugin : zcm::IndexerPlugin
{
  public:
    static IndexerPlugin* makeIndexerPlugin();

    virtual ~CustomIndexerPlugin();

    std::string name() const override;

    std::vector<std::string> dependencies() const override;

    std::vector<std::string> includeInIndex(std::string channel,
                                            std::string typeName,
                                            int64_t hash,
                                            const char* data,
                                            int32_t datalen) const override;
    bool sorted() const override;
    bool lessThan(off_t a, off_t b, zcm::LogFile& log,
                  const Json::Value& index) const override;
};

zcm::IndexerPlugin* CustomIndexerPlugin::makeIndexerPlugin()
{ return new CustomIndexerPlugin(); }

CustomIndexerPlugin::~CustomIndexerPlugin()
{}

std::string CustomIndexerPlugin::name() const
{ return "custom plugin"; }

std::vector<std::string> CustomIndexerPlugin::dependencies() const
{ return {"timestamp"}; }

std::vector<std::string> CustomIndexerPlugin::includeInIndex(std::string channel,
                                                             std::string typeName,
                                                             int64_t hash,
                                                             const char* data,
                                                             int32_t datalen) const
{
    example_t msg;
    if (!msg.decode(data, 0, datalen)) return {};
    // Could also do:
    // if (typeName != "example_t") return {};
    return {channel, typeName};
}

bool CustomIndexerPlugin::sorted() const
{ return true; }

bool CustomIndexerPlugin::lessThan(off_t a, off_t b, zcm::LogFile& log,
                                   const Json::Value& index) const
{
    example_t msgA, msgB;

    const zcm::LogEvent* evtA = log.readEventAtOffset(a);
    assert(evtA);
    assert(msgA.decode(evtA->data, 0, evtA->datalen));

    const zcm::LogEvent* evtB = log.readEventAtOffset(b);
    assert(evtB);
    assert(msgB.decode(evtB->data, 0, evtB->datalen));

    return msgA.position[0] > msgB.position[0];
}
