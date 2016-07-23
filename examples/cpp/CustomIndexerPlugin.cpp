#include <cassert>

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
    bool lessThan(int64_t hash, const char* a, int32_t aLen,
                                const char* b, int32_t bLen) const override;
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

bool CustomIndexerPlugin::lessThan(int64_t hash, const char* a, int32_t aLen,
                                                 const char* b, int32_t bLen) const
{
    example_t msgA, msgB;
    assert(msgA.decode(a, 0, aLen));
    assert(msgB.decode(b, 0, bLen));
    return msgA.position[0] >= msgB.position[0];
}
