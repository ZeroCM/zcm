#include <cassert>
#include <iostream>
#include <string>
#include <algorithm>

#include <zcm/IndexerPlugin.hpp>

#include "types/example_t.hpp"

// REMEMBER: Do not define any functions where they are declared in the class
// See IndexerPlugin.hpp for further explanation
class CustomIndexerPlugin : zcm::IndexerPlugin
{
  public:
    static IndexerPlugin* makeIndexerPlugin();

    virtual ~CustomIndexerPlugin();

    std::string name() const override;

    std::vector<std::string> dependsOn() const override;

    void setUp(const Json::Value& index, Json::Value& pluginIndex, zcm::LogFile& log) override;

    void indexEvent(const Json::Value& index, Json::Value& pluginIndex,
                    std::string channel, std::string typeName,
                    off_t offset, uint64_t timestamp, int64_t hash,
                    const char* data, int32_t datalen) override;

    void tearDown(const Json::Value& index, Json::Value& pluginIndex, zcm::LogFile& log) override;
};


zcm::IndexerPlugin* CustomIndexerPlugin::makeIndexerPlugin()
{ return new CustomIndexerPlugin(); }

CustomIndexerPlugin::~CustomIndexerPlugin()
{}

std::string CustomIndexerPlugin::name() const
{ return "custom plugin"; }

std::vector<std::string> CustomIndexerPlugin::dependsOn() const
{ return {"timestamp"}; }

void CustomIndexerPlugin::setUp(const Json::Value& index,
                                Json::Value& pluginIndex,
                                zcm::LogFile& log)
{ }

void CustomIndexerPlugin::indexEvent(const Json::Value& index, Json::Value& pluginIndex,
                                     std::string channel, std::string typeName,
                                     off_t offset, uint64_t timestamp, int64_t hash,
                                     const char* data, int32_t datalen)
{
    if (typeName != "example_t") return;
    pluginIndex[channel][typeName].append(std::to_string(offset));
}

void CustomIndexerPlugin::tearDown(const Json::Value& index,
                                   Json::Value& pluginIndex,
                                   zcm::LogFile& log)
{
    std::cout << "sorting " << name() << std::endl;

    fseeko(log.getFilePtr(), 0, SEEK_END);
    off_t logSize = ftello(log.getFilePtr());

    auto comparator = [&](off_t a, off_t b) {
        if (a < 0 || b < 0 || a > logSize || b > logSize) {
            std::cerr << "Sorting has failed. "
                      << "Sorting function is probably broken. "
                      << "Aborting." << std::endl;
            exit(1);
        }

        example_t msgA, msgB;

        const zcm::LogEvent* evtA = log.readEventAtOffset(a);
        assert(evtA);
        assert(msgA.decode(evtA->data, 0, evtA->datalen));

        const zcm::LogEvent* evtB = log.readEventAtOffset(b);
        assert(evtB);
        assert(msgB.decode(evtB->data, 0, evtB->datalen));

        return msgA.position[0] < msgB.position[0];
    };

    for (std::string channel : pluginIndex.getMemberNames()) {
        for (std::string type : pluginIndex[channel].getMemberNames()) {
            std::vector<off_t> offsets;
            for (size_t i = 0; i < pluginIndex[channel][type].size(); ++i) {
                std::string offset = pluginIndex[channel][type][(int)i].asString();
                size_t sz = 0;
                long long off = stoll(offset, &sz, 0);
                assert(sz < offset.length());
                offsets.push_back((off_t) off);
            }
            std::sort(offsets.begin(), offsets.end(), comparator);
            for (size_t i = 0; i < pluginIndex[channel][type].size(); ++i) {
                pluginIndex[channel][type][(int)i] = std::to_string(offsets[i]);
            }
        }
    }
}
