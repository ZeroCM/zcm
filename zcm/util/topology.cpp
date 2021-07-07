#include "topology.hpp"

#include <sstream>
#include <sys/stat.h>
#include <unistd.h>

#include <zcm/zcm.h>

#include "debug.h"

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

    if (filename[filename.size() - 1] != '/') filename += "/";

    filename += name + ".json";

    FILE *fd = fopen(filename.c_str(), "wb");
    if (!fd) return ZCM_EUNKNOWN;

    stringstream data;
    data << "{" << endl;

    data << "  \"name\": \"" << name << "\"," << endl;

    data << "  \"subscribes\": [ " << endl;
    for (auto chan : receivedTopologyMap) {
        data << "    {" << endl;
        data << "      \"" << chan.first << "\": [ " << endl;
        for (auto type : chan.second) {
            data << "        { "
                 << "\"BE\": \"" << type.second.first << "\", "
                 << "\"LE\": \"" << type.second.second << "\""
                 << " }," << endl;
        }
        data.seekp(-2, data.cur);
        data << endl;
        data << "      ]" << endl;
        data << "    }," << endl;
    }
    data.seekp(-2, data.cur);
    data << endl;
    data << "  ]," << endl;

    data << "  \"publishes\": [ " << endl;
    for (auto chan : sentTopologyMap) {
        data << "    {" << endl;
        data << "      \"" << chan.first << "\": [ " << endl;
        for (auto type : chan.second) {
            data << "        { "
                 << "\"BE\": \"" << type.second.first << "\", "
                 << "\"LE\": \"" << type.second.second << "\""
                 << " }," << endl;
        }
        data.seekp(-2, data.cur);
        data << endl;
        data << "      ]" << endl;
        data << "    }," << endl;
    }
    data.seekp(-2, data.cur);
    data << endl;
    data << "  ]" << endl;

    data << "}" << endl;

    err = fwrite(data.str().c_str(), 1, data.str().size(), fd);
    if (err < 0) return ZCM_EUNKNOWN;

    err = fclose(fd);
    if (err < 0) return ZCM_EUNKNOWN;

    return ZCM_EOK;
}

} /* zcm */
