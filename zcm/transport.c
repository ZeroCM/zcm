#include "zcm/transport.h"
#include <string.h>

#define ZCM_TRANSPORTS_MAX 128
static const char *t_name[ZCM_TRANSPORTS_MAX];
static const char *t_desc[ZCM_TRANSPORTS_MAX];
static zcm_trans_create_func *t_creator[ZCM_TRANSPORTS_MAX];
static size_t t_index = 0;

bool zcm_transport_register(const char *name, const char *desc, zcm_trans_create_func *creator)
{
    if (t_index >= ZCM_TRANSPORTS_MAX)
        return false;

    // Does this name already exist?
    for (size_t i = 0; i < t_index; i++)
        if (strcmp(t_name[i], name) == 0)
            return false;

    t_name[t_index] = name;
    t_desc[t_index] = desc;
    t_creator[t_index] = creator;
    t_index++;
    return true;
}

zcm_trans_create_func *zcm_transport_find(const char *name)
{
    for (size_t i = 0; i < t_index; i++)
        if (strcmp(t_name[i], name) == 0)
            return t_creator[i];
    return NULL;
}

void zcm_transport_list(FILE *f)
{
    fprintf(f, "Transport Name       Description\n");
    fprintf(f, "-------------------------------------------------------------------------------------\n");
    for (size_t i = 0; i < t_index; i++) {
        fprintf(f, "%-20s %s\n", t_name[i], t_desc[i]);
    }
}
