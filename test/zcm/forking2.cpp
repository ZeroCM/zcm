#include "zcm/zcm.h"
#include <unistd.h>
#include <sys/wait.h>

#define CHANNEL "TEST_CHANNEL"

static size_t numrecv = 0;
static void handler(const zcm_recv_buf_t *rbuf, const char *channel, void *usr)
{
    numrecv++;
}

static void handle(zcm_t *zcm)
{
    int rc = zcm_handle(zcm);
    if (rc == -1)  {
        printf("handle() failed!\n");
        exit(1);
    }
}

bool sub(zcm_t *zcm)
{
    zcm_subscribe(zcm, CHANNEL, handler, NULL);

    zcm_start(zcm);
    usleep(500000);
    zcm_stop(zcm);

    return numrecv == 1;
}

void pub(zcm_t *zcm)
{
    // Sleep for a moment to give the sub() process time to start
    usleep(100000);

    char data = 'd';
    zcm_publish(zcm, CHANNEL, &data, 1);
    usleep(200000);
    zcm_publish(zcm, CHANNEL, &data, 1);
    usleep(10000);
}

int main()
{
    zcm_t *zcm = zcm_create("ipc");
    char data = 'd';
    zcm_publish(zcm, CHANNEL, &data, 1);

    pid_t pid;
    pid = ::fork();

    if (pid < 0) {
        printf("Fork failed!\n");
        exit(1);
    }

    else if (pid == 0) {
        pub(zcm);
    }

    else {
        bool success = sub(zcm);
        int status;
        ::waitpid(-1, &status, 0);
        if (success) {
            return 0;
        } else {
            printf("Failed!\n");
            return 1;
        }
    }

    zcm_destroy(zcm);
    return 0;
}
