#pragma once
#include "Common.hpp"
#include "util/TypeDb.hpp"
#include "ExpiringQueue.hpp"
#include "MsgDisplay.hpp"

class MsgInfo
{
    static constexpr size_t QUEUE_SIZE = 400;
    static constexpr size_t QUEUE_PERIOD = 4*1000*1000;

public:
    MsgInfo(zcm::TypeDb& db, const char *channel);
    ~MsgInfo();

    void addMessage(u64 utime, const zcm_recv_buf_t *rbuf);
    float getHertz();
    float getBandwidthBps();
    u64 getNumMsgs() { return num_msgs; }

    size_t getViewDepth();
    void incViewDepth(size_t viewid, const string& return_prefix_filter);
    string decViewDepth();

    void display(const string& active_prefix_filter);

private:
    void ensureHash(i64 hash);
    u64 latestUtime();
    u64 oldestUtime();
    void removeOld();

private:
    zcm::TypeDb& db;
    string channel;

    // utime -> data size in bytes
    ExpiringQueue<std::pair<uint64_t, size_t>, QUEUE_SIZE> queue;
    i64 hash = 0;
    u64 num_msgs = 0;

    void *last_msg = NULL;
    const zcm::TypeMetadata *metadata = NULL;
    MsgDisplayState disp_state;
};
