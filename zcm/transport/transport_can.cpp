#include "zcm/transport.h"
#include "zcm/transport_registrar.h"
#include "zcm/transport_register.hpp"
#include "zcm/util/debug.h"

#include "generic_serial_transport.h"

#include "util/TimeUtil.hpp"

#include <unistd.h>
#include <string.h>
#include <iostream>
#include <limits>
#include <cstdio>
#include <cassert>
#include <unordered_map>
#include <algorithm>

#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <linux/sockios.h>

#include <linux/can.h>
#include <linux/can/raw.h>

#define MASK_29B ((1 << 29) - 1)
#define MASK_11B ((1 << 11) - 1)

// Define this the class name you want
#define ZCM_TRANS_CLASSNAME TransportCan
#define MTU (1<<14)

using namespace std;

struct ZCM_TRANS_CLASSNAME : public zcm_trans_t
{
    unordered_map<string, string> options;
    uint32_t msgId;
    uint32_t txId;
    string address;

    int soc = -1;
    bool socSettingsGood = false;
    struct sockaddr_can addr;
	struct ifreq ifr;

    zcm_trans_t* gst = nullptr;

    uint64_t recvTimeoutUs = 0;
    uint64_t recvMsgStartUtime = 0;

    uint8_t leftoverBuffer[CAN_MAX_DLEN];
    size_t leftoverBytes = 0;
    size_t leftoverOffset = 0;

    string* findOption(const string& s)
    {
        auto it = options.find(s);
        if (it == options.end()) return nullptr;
        return &it->second;
    }

    ZCM_TRANS_CLASSNAME(zcm_url_t* url)
    {
        trans_type = ZCM_BLOCKING;
        vtbl = &methods;

        // build 'options'
        auto* opts = zcm_url_opts(url);
        for (size_t i = 0; i < opts->numopts; ++i)
            options[opts->name[i]] = opts->value[i];

        msgId = 0;
        auto* msgIdStr = findOption("msgid");
        if (!msgIdStr) {
            ZCM_DEBUG("Msg Id unspecified");
            return;
        } else {
            char *endptr;
            msgId = (strtoul(msgIdStr->c_str(), &endptr, 10) & MASK_29B);
            if (*endptr != '\0') {
                ZCM_DEBUG("Msg Id unspecified");
                return;
            }
        }

        auto* txAddrMode = findOption("tx_addr_mode");
        bool extendedTx;
        if (!txAddrMode || string("extended") == txAddrMode->c_str()) {
            extendedTx = true;
        } else if (string("standard") == txAddrMode->c_str()) {
            extendedTx = false;
        } else {
            ZCM_DEBUG("Invalid rx_addr_mode. Use 'extended' or 'standard'");
            return;
        }
        if (!extendedTx && ((msgId & MASK_11B) != msgId)) {
            ZCM_DEBUG("Msg Id too long for standard can addresses. "
                      "Use 'tx_extended_addr=true'");
            return;
        }
        if (extendedTx && ((msgId & MASK_29B) != msgId)) {
            ZCM_DEBUG("Msg Id too long for extended can addresses.");
            return;
        }
        txId = extendedTx ? msgId | CAN_EFF_FLAG : msgId;

        struct can_filter rfilter[2];
        size_t numRxFilters = 0;

        auto* rxAddrMode = findOption("rx_addr_mode");
        if (!rxAddrMode || string("both") == rxAddrMode->c_str()) {
            rfilter[0].can_id   = msgId;
            rfilter[0].can_mask = (CAN_EFF_FLAG | CAN_RTR_FLAG | CAN_SFF_MASK);
            rfilter[1].can_id   = msgId | CAN_EFF_FLAG;
            rfilter[1].can_mask = (CAN_EFF_FLAG | CAN_RTR_FLAG | CAN_EFF_MASK);
            numRxFilters = 2;
        } else if (string("standard") == rxAddrMode->c_str()) {
            rfilter[0].can_id   = msgId;
            rfilter[0].can_mask = (CAN_EFF_FLAG | CAN_RTR_FLAG | CAN_SFF_MASK);
            numRxFilters = 1;
        } else if (string("extended") == rxAddrMode->c_str()) {
            rfilter[0].can_id   = msgId | CAN_EFF_FLAG;
            rfilter[0].can_mask = (CAN_EFF_FLAG | CAN_RTR_FLAG | CAN_EFF_MASK);
            numRxFilters = 1;
        } else {
            ZCM_DEBUG("Invalid rx_addr_mode. Use 'extended', 'standard', or 'both'");
            return;
        }

        address = zcm_url_address(url);

        if ((soc = socket(PF_CAN, SOCK_RAW, CAN_RAW)) < 0) {
            ZCM_DEBUG("Unable to make socket");
		    return;
	    }

        strcpy(ifr.ifr_name, address.c_str());
        ioctl(soc, SIOCGIFINDEX, &ifr);

        memset(&addr, 0, sizeof(addr));
        addr.can_family = AF_CAN;
        addr.can_ifindex = ifr.ifr_ifindex;

        if (bind(soc, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
            ZCM_DEBUG("Failed to bind");
            close(soc);
            return;
        }

        if (setsockopt(soc, SOL_CAN_RAW, CAN_RAW_FILTER, &rfilter,
                       numRxFilters * sizeof(struct can_filter)) < 0) {
            ZCM_DEBUG("Failed to set filter");
            return;
        }

        gst = zcm_trans_generic_serial_create(&ZCM_TRANS_CLASSNAME::get,
                                              &ZCM_TRANS_CLASSNAME::put,
                                              this,
                                              &ZCM_TRANS_CLASSNAME::timestamp_now,
                                              this,
                                              MTU, MTU * 10);
        socSettingsGood = true;
    }

    ~ZCM_TRANS_CLASSNAME()
    {
        if (gst) zcm_trans_generic_serial_destroy(gst);
        if (soc != -1 && close(soc) < 0) {
            ZCM_DEBUG("Failed to close");
	    }
        soc = -1;
    }

    bool good()
    {
        return soc != -1 && socSettingsGood;
    }

    static size_t get(uint8_t* data, size_t nData, void* usr)
    {
        ZCM_TRANS_CLASSNAME* me = cast((zcm_trans_t*) usr);
        size_t totalRead = 0;

        if (me->leftoverBytes > 0) {
            size_t availableLeftover = me->leftoverBytes - me->leftoverOffset;
            size_t fromLeftover = min(nData, availableLeftover);
            memcpy(data, &me->leftoverBuffer[me->leftoverOffset], fromLeftover);
            me->leftoverOffset += fromLeftover;
            totalRead += fromLeftover;

            if (me->leftoverOffset >= me->leftoverBytes) {
                me->leftoverBytes = 0;
                me->leftoverOffset = 0;
            }

            if (totalRead >= nData)  return totalRead;
        }

        struct can_frame frame;
        int nbytes = read(me->soc, &frame, sizeof(struct can_frame));
        if (nbytes != sizeof(struct can_frame)) {
            // Sleeping if read returned an error in case read didn't
            // expire all the remaining timeout
            if (nbytes < 0) {
                uint64_t timeoutConsumedUs = TimeUtil::utime() - me->recvMsgStartUtime;
                if (timeoutConsumedUs >= me->recvTimeoutUs) return totalRead;
                usleep(me->recvTimeoutUs - timeoutConsumedUs);
            }
            return totalRead;
        }

        // Handle the new frame data
        size_t frameDataSize = (size_t) frame.can_dlc;
        nData -= totalRead;
        size_t fromNewFrame = min(nData, frameDataSize);
        memcpy(&data[totalRead], frame.data, fromNewFrame);
        totalRead += fromNewFrame;

        if (fromNewFrame < frameDataSize) {
            me->leftoverBytes = frameDataSize - fromNewFrame;
            me->leftoverOffset = 0;
            memcpy(me->leftoverBuffer, (uint8_t*)frame.data + fromNewFrame, me->leftoverBytes);
        }

        return totalRead;
    }

    static size_t sendFrame(const uint8_t* data, size_t nData, void* usr)
    {
        ZCM_TRANS_CLASSNAME* me = cast((zcm_trans_t*) usr);

        struct can_frame frame;
        frame.can_id = me->txId;

        size_t ret = min(nData, (size_t) CAN_MAX_DLEN);
        frame.can_dlc = ret;
        memcpy(frame.data, data, ret);

        if (write(me->soc, &frame, sizeof(struct can_frame)) != sizeof(struct can_frame)) {
            ZCM_DEBUG("Failed to write data");
            return 0;
        }

        return ret;
    }

    static size_t put(const uint8_t* data, size_t nData, void* usr)
    {
        size_t ret = 0;
        while (ret < nData) {
            size_t left = nData - ret;
            size_t written = sendFrame(&data[ret], left, usr);
            if (written == 0) return ret;
            ret += written;
        }
        return ret;
    }

    static uint64_t timestamp_now(void* usr)
    {
        ZCM_TRANS_CLASSNAME* me = cast((zcm_trans_t*) usr);

        struct timeval time;
        if (ioctl(me->soc, SIOCGSTAMP, &time) == -1) return 0;
        return time.tv_sec * 1e6 + time.tv_usec;
    }

    /********************** METHODS **********************/
    size_t getMtu()
    {
        return zcm_trans_get_mtu(this->gst);
    }

    int sendmsg(zcm_msg_t msg)
    {
        int ret = zcm_trans_sendmsg(this->gst, msg);
        if (ret != ZCM_EOK) return ret;
        return serial_update_tx(this->gst);
    }

    int recvmsgEnable(const char* channel, bool enable)
    {
        return zcm_trans_recvmsg_enable(this->gst, channel, enable);
    }

    int recvmsg(zcm_msg_t* msg, unsigned timeoutMs)
    {
        recvMsgStartUtime = TimeUtil::utime();
        recvTimeoutUs = timeoutMs * 1000;

        do {
            int ret = zcm_trans_recvmsg(this->gst, msg, 0);
            if (ret == ZCM_EOK) return ret;

            uint64_t timeoutConsumedUs = TimeUtil::utime() - recvMsgStartUtime;
            if (timeoutConsumedUs > recvTimeoutUs) break;

            uint64_t socketTimeout = recvTimeoutUs - timeoutConsumedUs;
            unsigned timeoutS = socketTimeout / 1000000;
            unsigned timeoutUs = socketTimeout - timeoutS * 1000000;
            struct timeval tm = {
                timeoutS,  /* seconds */
                timeoutUs, /* micros */
            };
            if (setsockopt(soc, SOL_SOCKET, SO_RCVTIMEO, (char *)&tm, sizeof(tm)) < 0) {
                ZCM_DEBUG("Failed to settimeout");
                return ZCM_EUNKNOWN;
            }

            serial_update_rx(this->gst);
        } while (true);
        return ZCM_EAGAIN;
    }

    int setQueueSize(unsigned numMsgs)
    {
        // kernel buffers have 2x overhead on buffers for internal bookkeeping
        int recvQueueSize = getMtu() * numMsgs * 2;
        if (setsockopt(soc, SOL_SOCKET, SO_RCVBUF,
                       (void *)&recvQueueSize, sizeof(recvQueueSize)) < 0) {
            return ZCM_EUNKNOWN;
        }
        return ZCM_EOK;
    }

    /********************** STATICS **********************/
    static zcm_trans_methods_t methods;
    static ZCM_TRANS_CLASSNAME *cast(zcm_trans_t *zt)
    {
        assert(zt->vtbl == &methods);
        return (ZCM_TRANS_CLASSNAME*)zt;
    }

    static size_t _getMtu(zcm_trans_t *zt)
    { return cast(zt)->getMtu(); }

    static int _sendmsg(zcm_trans_t *zt, zcm_msg_t msg)
    { return cast(zt)->sendmsg(msg); }

    static int _recvmsgEnable(zcm_trans_t *zt, const char *channel, bool enable)
    { return cast(zt)->recvmsgEnable(channel, enable); }

    static int _recvmsg(zcm_trans_t *zt, zcm_msg_t *msg, unsigned timeout)
    { return cast(zt)->recvmsg(msg, timeout); }

    static int _setQueueSize(zcm_trans_t *zt, unsigned numMsgs)
    { return cast(zt)->setQueueSize(numMsgs); }

    static void _destroy(zcm_trans_t *zt)
    { delete cast(zt); }

    /** If you choose to use the registrar, use a static registration member **/
    static const TransportRegister reg;
};

zcm_trans_methods_t ZCM_TRANS_CLASSNAME::methods = {
    &ZCM_TRANS_CLASSNAME::_getMtu,
    &ZCM_TRANS_CLASSNAME::_sendmsg,
    &ZCM_TRANS_CLASSNAME::_recvmsgEnable,
    &ZCM_TRANS_CLASSNAME::_recvmsg,
    NULL, // drops
    &ZCM_TRANS_CLASSNAME::_setQueueSize,
    NULL, // update
    &ZCM_TRANS_CLASSNAME::_destroy,
};

static zcm_trans_t *create(zcm_url_t* url, char **opt_errmsg)
{
    if (opt_errmsg) *opt_errmsg = NULL; // Feature unused in this transport

    auto* trans = new ZCM_TRANS_CLASSNAME(url);
    if (trans->good())
        return trans;

    delete trans;
    return nullptr;
}

#ifdef USING_TRANS_CAN
const TransportRegister ZCM_TRANS_CLASSNAME::reg(
    "can", "Transfer data via a socket CAN connection on a single id "
           "(e.g. 'can://can0?msgid=65536&rx_extended_addr=standard&tx_extended_addr=true')", create);
#endif
