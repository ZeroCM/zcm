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
    b.field6 = 1;
    b.field7 = 3;
    b.field9 = 1 << 27;
    b.field10 = ((uint64_t)1 << 52) | 1;
    b.field11 = 3;
    for (size_t i = 0; i < 3; ++i) {
        for (size_t j = 0; j < 2; ++j) {
            for (size_t k = 0; k < 2; ++k) {
                for (size_t l = 0; l < 2; ++l) {
                    b.field12[i][j][k][l] = k + l;
                }
            }
        }
    }
    b.field15 = 0b1000100;
    b.field16 = 0b0000010;
    b.field19 = 0b1000100;
    b.field20 = 0b0000010;
    b.field22 = b.FIELD22_TEST;
    b.field23 = b.FIELD23_TEST;
    b.field24 = b.FIELD24_TEST;
    b.field25 = 0xff;
    b.field26 = 0xff;
    b.field27 = 0xff;
    b.field28 = 0xff;
    b.field29 = 0xffff;
    b.field30 = 0xffff;
    b.field31 = 0xffffffff;
    b.field32 = 0xffffffff;
    b.field33 = 0xffffffffffffffff;
    b.field34 = 0xffffffffffffffff;

    while (1) {
        zcm.publish("BITFIELD", &b);
        usleep(periodUs);
    }

    return 0;
}
