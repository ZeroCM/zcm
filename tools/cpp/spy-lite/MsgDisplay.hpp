#pragma once
#include "util/TypeDb.hpp"
#include "Common.hpp"

#define MSG_DISPLAY_RECUR_MAX 64
struct MsgDisplayState
{
    size_t cur_depth = 0;
    size_t recur_table[MSG_DISPLAY_RECUR_MAX];
};

void msg_display(TypeDb& db, const TypeMetadata& metadata, void *msg, const MsgDisplayState& state);
