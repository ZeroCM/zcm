#include <stdio.h>
#include <unistd.h>
#include <string>
#include <zcm/zcm-cpp.hpp>
#include "types/bitfield_t.hpp"
using std::string;

class Handler
{
    public:
        ~Handler() {}

        void handleMessage(const zcm::ReceiveBuffer* rbuf,
                           const string& chan,
                           const bitfield_t *msg)
        {
            printf("Received message on channel \"%s\":\n", chan.c_str());
            printf("%d\n", msg->field1);
            printf("[");
            for (int j = 0; j < 2; ++j) {
                printf("[");
                for (int i = 0; i < 4; ++i) {
                    printf("%d, ", msg->field2[j][i]);
                }
                printf("], ");
            }
            printf("]\n");
            printf("%d\n", msg->field3);
            printf("%d\n", msg->field4);
            printf("%d\n", msg->field5);
            printf("%d\n", msg->field6);
            printf("%d\n", msg->field7);
            printf("%d\n", msg->field8_dim1);
            printf("%d\n", msg->field8_dim2);
            printf("[");
            for (int j = 0; j < msg->field8_dim1; ++j) {
                printf("[");
                for (int i = 0; i < msg->field8_dim2; ++i) {
                    printf("%d, ", msg->field8[j][i]);
                }
                printf("]");
            }
            printf("]\n");
            printf("%d\n", msg->field9);
            printf("%ld\n", msg->field10);
            printf("%d\n", msg->field11);
            printf("[");
            for (int l = 0; l < 3; ++l) {
                printf("[");
                for (int k = 0; k < 2; ++k) {
                    printf("[");
                    for (int j = 0; j < 2; ++j) {
                        printf("[");
                        for (int i = 0; i < 2; ++i) {
                            printf("%d, ", msg->field12[l][k][j][i]);
                        }
                        printf("], ");
                    }
                    printf("], ");
                }
                printf("], ");
            }
            printf("]\n");
        }
};

int main(int argc, char *argv[])
{
    zcm::ZCM zcm {""};
    if (!zcm.good())
        return 1;

    Handler handlerObject;
    zcm.subscribe("BITFIELD", &Handler::handleMessage, &handlerObject);
    zcm.run();

    return 0;
}
