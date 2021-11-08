#include <unistd.h>

#include <zcm/zcm-cpp.hpp>

#include "types/bitfield_t.hpp"

static int HZ = 1;

int main(int argc, char *argv[])
{
    uint64_t periodUs = 1e6 / HZ;

    zcm::ZCM zcm {""};
    if (!zcm.good())
        return 1;

    bitfield_t b = {};
    b.field1 = 3;
    b.field2[0][0] = 1;
    b.field2[0][1] = 0;
    b.field2[0][2] = 1;
    b.field2[0][3] = 0;
    b.field2[1][0] = 1;
    b.field2[1][1] = 0;
    b.field2[1][2] = 1;
    b.field2[1][3] = 0;
    b.field3 = 0xf;
    b.field4 = 5;
    b.field5 = 7;
    b.field9 = 1 << 27;
    b.field10 = ((uint64_t)1 << 52) | 1;

    while (1) {
        zcm.publish("BITFIELD", &b);
        usleep(periodUs);
    }

    return 0;
}
