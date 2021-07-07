#include "topology.hpp"

#include <fstream>
#include <sstream>
#include <sys/stat.h>
#include <unistd.h>

#include <zcm/zcm.h>

#include "debug.h"
#include "zcm/json/json.h"

using namespace std;

namespace zcm {

int writeTopology(string name,
                  const TopologyMap& receivedTopologyMap,
                  const TopologyMap& sentTopologyMap)
{
    string filename = "/tmp/zcm_topology/";
    const char* dir = std::getenv("ZCM_TOPOLOGY_DIR");
    if (dir) filename = dir;

    int err = mkdir(filename.c_str(), S_IRWXO | S_IRWXG | S_IRWXU);
    if (err < 0 && errno != EEXIST) {
        ZCM_DEBUG("Unable to create lockfile directory");
        return ZCM_EUNKNOWN;
    }

    zcm::Json::Value json;
    json["name"] = name;

    int i;

    i = 0;
    for (auto chan : receivedTopologyMap) {
        for (auto type : chan.second) {
            json["publishes"][i]["BE"] = zcm::Json::Int64(type.second.first);
            json["publishes"][i]["LE"] = zcm::Json::Int64(type.second.second);
        }
        ++i;
    }

    i = 0;
    for (auto chan : sentTopologyMap) {
        for (auto type : chan.second) {
            json["publishes"][i]["BE"] = zcm::Json::Int64(type.second.first);
            json["publishes"][i]["LE"] = zcm::Json::Int64(type.second.second);
        }
        ++i;
    }


    if (filename[filename.size() - 1] != '/') filename += "/";
    filename += name + ".json";
    ofstream outfile{ filename };

    if (!outfile.good()) return ZCM_EUNKNOWN;

    outfile << json << endl;

    outfile.close();

    return ZCM_EOK;
}

} /* zcm */
