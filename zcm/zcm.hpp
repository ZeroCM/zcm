#include "zcm.h"

class ZCM
{
    ZCM()  { zcm = zcm_create(); }
    ~ZCM() { zcm_destroy(zcm); }

    bool good() { return zcm != NULL; }

    int publish(const std::string& channel, char *data, size_t len)
    {
        return zcm_publish(zcm, channel.c_str(), data, len);
    }

    int subscribe(const std::string& channel, zcm_callback_t *cb, void *usr)
    {
        zcm_subscribe(zcm, channel.c_str(), cb, usr);
    }

  private:
    zcm_t *zcm;
};
