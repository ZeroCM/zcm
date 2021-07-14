#pragma once

#include <string>
#include <unordered_map>
#include <utility>

namespace zcm {

// RRR: it seems a bit repetitive to have BE hash in here twice. You could instead use
//      std::unordered_map<std::string, std::unordered_set<std::pair<int64_t, int64_t>>>
//      To do that, you'll have to specialize the hash function, but because you know
//      the first and second elements are always related, you could just use the hash
//      of the first element of the pair, so that'd be easy
// channel_name -> (big_endian_type_hash -> { big_endian_type_hash, little_endian_type_hash })
typedef std::unordered_map<std::string, std::unordered_map<int64_t, std::pair<int64_t,int64_t>>> TopologyMap;

int writeTopology(std::string name, const TopologyMap& receivedTopologyMap, const TopologyMap& sentTopologyMap);

} /* zcm */
