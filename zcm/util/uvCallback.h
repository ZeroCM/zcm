#ifndef _CB_TEST_H
#define _CB_TEST_H

#include "zcm/zcm.h"

#ifdef __cplusplus
extern "C" {
#endif

struct uvCallback;

uvCallback* uvCallbackCreate();
void        uvCallbackTrigger(uvCallback* uvCb, void (*cb)(void* usr), void* usr);
void        uvCallbackDestroy(uvCallback* uvCb);

#ifdef __cplusplus
}
#endif

#endif /* _CB_TEST_H */
