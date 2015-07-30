#ifndef _ZCM_TRANS_REG_H
#define _ZCM_TRANS_REG_H

#include "zcm/transport.h"
#include "zcm/url.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Functions that create zcm_trans_t should conform to this type signature */
typedef zcm_trans_t *(zcm_trans_create_func)(zcm_url_t *url);
bool zcm_transport_register(const char *name, const char *desc, zcm_trans_create_func *creator);
zcm_trans_create_func *zcm_transport_find(const char *name);
void zcm_transport_help(FILE *f);

#ifdef __cplusplus
}
#endif

#endif  /* _ZCM_TRANS_REG_H */
