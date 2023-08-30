#pragma once
#include <vector>
#include <utility>

#include "util/TypeDb.hpp"
#include "Common.hpp"

struct MsgDisplayState
{
    std::vector<std::pair<size_t, string>> recur_table;
};

void msg_display(zcm::TypeDb& db, const zcm::TypeMetadata& metadata,
                 void *msg, const MsgDisplayState& state,
                 const string& active_prefix_filter);
