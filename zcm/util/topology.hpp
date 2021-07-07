#pragma once

#include <string>
#include <unordered_map>
#include <utility>

namespace zcm {

// channel_name -> (big_endian_type_hash -> { big_endian_type_hash, little_endian_type_hash })
typedef std::unordered_map<std::string, std::unordered_map<int64_t, std::pair<int64_t,int64_t>>> TopologyMap;

int writeTopology(std::string name, const TopologyMap& receivedTopologyMap, const TopologyMap& sentTopologyMap);

} /* zcm */
