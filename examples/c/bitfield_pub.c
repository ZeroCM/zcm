#include <stdio.h>
#include <unistd.h>

#include <zcm/zcm.h>

#include "types/bitfield_t.h"

static int HZ = 1;

int main(int argc, char *argv[])
{
    uint64_t periodUs = 1e6 / HZ;

    zcm_t *zcm = zcm_create("");
    if (!zcm)
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
    b.field15 = 0b1000100;
    b.field16 = 0b0000010;
    b.field19 = 0b1000100;
    b.field20 = 0b0000010;
    b.field22 = BITFIELD_T_FIELD22_TEST;
    b.field23 = BITFIELD_T_FIELD23_TEST;
    b.field24 = BITFIELD_T_FIELD24_TEST;
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
        bitfield_t_publish(zcm, "BITFIELD", &b);
        usleep(periodUs);
    }

    zcm_destroy(zcm);
    return 0;
}
