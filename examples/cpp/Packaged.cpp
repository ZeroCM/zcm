#include <unistd.h>
#include <zcm/zcm-cpp.hpp>
#include "types/test_package/packaged_t.hpp"

static constexpr uint8_t HZ = 1;

int main(int argc, char *argv[])
{
    zcm::ZCM zcm {""};
    if (!zcm.good())
        return 1;

    test_package::packaged_t my_data {};

    my_data.packaged = true;
    my_data.a.packaged = true;
    my_data.a.e.timestamp = 0;
    my_data.a.e.p.packaged = true;

    while (1) {
        zcm.publish("PACKAGED", &my_data);
        ++my_data.a.e.timestamp;
        usleep(1000000 / HZ);
    }

    return 0;
}
