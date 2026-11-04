#pragma once
#include "udp.hpp"

#include <atomic>

/**
 * Counts messages that a UDP receiver lost, using the per-sender msg_seqno
 * that every ZCM UDP packet carries (short and fragmented messages share one
 * rolling counter per sending socket).
 *
 * A message is counted as dropped exactly once, when we find out about it:
 *   - none of its packets arrived (a gap in the sender's sequence numbers), or
 *   - some of its fragments arrived but it could not be reassembled
 *     (missing fragments, an invalid fragment, or the fragment buffer was
 *     evicted).
 *
 * Each sender keeps a 64-message window of sequence numbers that have already
 * been accounted for (delivered or counted as dropped) so that late or
 * duplicated packets do not count the same message twice. The total only ever
 * increases, so callers can safely take deltas between queries.
 *
 * Not thread safe except for getDrops(), which may be called from any thread.
 */
class LossTracker
{
  public:
    // Packets more than this far behind the newest sequence number seen from a
    // sender are assumed to come from a restarted sender, not reordering.
    static constexpr u32 RESTART_THRESHOLD = 4096;
    // Bound the per-sender state; if exceeded we forget all senders, which can
    // only cause missed drops, never false ones.
    static constexpr size_t MAX_SENDERS = 1024;

    // Call for every well-formed ZCM packet before it is processed.
    void observe(const struct sockaddr_in *from, u32 seqno)
    {
        u64 key = makeKey(from);
        Sender *sp = find(key);
        if (!sp) {
            if (senders.size() >= MAX_SENDERS) {
                senders.clear();
                cachedSender = nullptr;
            }
            Sender s;
            s.newest = seqno;
            senders.emplace(key, s);
            return;
        }

        Sender& s = *sp;
        u32 ahead = seqno - s.newest;
        if (ahead == 0) return;

        if (ahead < 0x80000000u) {
            // Newer message. Everything strictly between the newest seqno and
            // this one never showed up at all.
            u64 missing = ahead - 1;
            if (missing > 0) drops += missing;

            if (ahead >= WINDOW) {
                s.accounted = 0;
            } else {
                s.accounted <<= ahead;
            }
            // Bits 1..min(missing,63) represent the messages counted above
            if (missing > 0) {
                u64 nbits = std::min<u64>(missing, WINDOW - 1);
                u64 mask = ((1ull << nbits) - 1) << 1;
                s.accounted |= mask;
            }
            s.newest = seqno;
            return;
        }

        u32 behind = s.newest - seqno;
        if (behind > RESTART_THRESHOLD) {
            // Sender most likely restarted and reset its counter
            s.newest = seqno;
            s.accounted = 0;
        }
        // Otherwise it's a late (reordered or duplicated) packet; there is
        // nothing to count, markDropped/markDelivered handle the rest.
    }

    // The message (sender, seqno) could not be delivered.
    void markDropped(const struct sockaddr_in *from, u32 seqno)
    {
        u64 *bit = nullptr;
        u64 mask = 0;
        if (!lookupBit(from, seqno, bit, mask)) {
            // Too old to deduplicate (or unknown sender): count it anyway
            drops++;
            return;
        }
        if (*bit & mask) return; // already accounted for
        *bit |= mask;
        drops++;
    }

    // The message (sender, seqno) was handed to the user.
    void markDelivered(const struct sockaddr_in *from, u32 seqno)
    {
        u64 *bit = nullptr;
        u64 mask = 0;
        if (lookupBit(from, seqno, bit, mask)) *bit |= mask;
    }

    uint64_t getDrops() const { return drops.load(std::memory_order_relaxed); }

  private:
    static constexpr u32 WINDOW = 64;

    struct Sender
    {
        u32 newest = 0;
        // bit i set => message (newest - i) has been delivered or counted
        u64 accounted = 0;
    };

    static u64 makeKey(const struct sockaddr_in *from)
    {
        return ((u64)from->sin_addr.s_addr << 16) | (u64)from->sin_port;
    }

    // Packets mostly arrive in runs from the same sender, so remember the
    // last one looked up. Pointers into an unordered_map stay valid until
    // the element is erased (we only ever clear()).
    Sender *find(u64 key)
    {
        if (cachedSender && cachedKey == key) return cachedSender;
        auto it = senders.find(key);
        if (it == senders.end()) return nullptr;
        cachedKey = key;
        cachedSender = &it->second;
        return cachedSender;
    }

    bool lookupBit(const struct sockaddr_in *from, u32 seqno, u64*& bits, u64& mask)
    {
        Sender *s = find(makeKey(from));
        if (!s) return false;
        u32 behind = s->newest - seqno;
        if (behind >= WINDOW) return false;
        bits = &s->accounted;
        mask = 1ull << behind;
        return true;
    }

    unordered_map<u64, Sender> senders;
    u64 cachedKey = 0;
    Sender *cachedSender = nullptr;
    std::atomic<uint64_t> drops {0};
};
