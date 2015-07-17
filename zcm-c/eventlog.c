#include "eventlog.h"

struct _zcm_eventlog_t
{
    FILE *f;
    int64_t eventcount;
};
