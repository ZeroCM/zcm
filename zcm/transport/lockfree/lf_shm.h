#pragma once
#include <stdbool.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

enum {
      LF_SHM_SUCCESS,
      LF_SHM_ERR_OPEN,
      LF_SHM_ERR_STAT,
      LF_SHM_ERR_MMAP,
      LF_SHM_ERR_SIZE,
      LF_SHM_ERR_MLOCK,
};

enum {
      LF_SHM_FLAG_MLOCK = 1<<0,
};

bool   lf_shm_create(const char *path, size_t size);
void   lf_shm_remove(const char *path);
void * lf_shm_open(const char *path, int flags, size_t * _opt_size, int *_opt_err);
void   lf_shm_close(void *shm, size_t size);

const char *lf_shm_errstr(int err);

#ifdef __cplusplus
}
#endif
