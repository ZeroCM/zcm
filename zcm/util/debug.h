#ifndef _ZCM_DEBUG_H
#define _ZCM_DEBUG_H

#include <stdlib.h>

#define ZCM_DEBUG_ENV_ENABLE

#ifdef ZCM_DEBUG_ENV_ENABLE
extern bool ZCM_DEBUG_ENABLED;
# define ZCM_DEBUG(...) do { \
    if (ZCM_DEBUG_ENABLED) {\
        fprintf(stderr, "ZCM-DEBUG: ");\
        fprintf(stderr, __VA_ARGS__);\
        fprintf(stderr, "\n");\
        fflush(stderr);\
    }} while(0)
#else
# define ZCM_DEBUG(...)
#endif

#endif
