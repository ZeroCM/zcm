#pragma once
#include <stdbool.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

bool   lf_shm_create(const char *path, size_t size);
void   lf_shm_remove(const char *path);
void * lf_shm_open(const char *path, size_t * _opt_size);
void   lf_shm_close(void *shm, size_t size);

#ifdef __cplusplus
}
#endif
