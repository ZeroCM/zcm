#ifndef ZCM_LOCKFILE
#define ZCM_LOCKFILE

// This is an API for creating and reclaiming lockfiles under
// a linux (and posibly posix) system. This utility allows
// posix transport layers to restrict multiple programs from
// opening the same transport-endpoint (for those endpoints
// that must be uniquely used). This API also supports checking
// for the case where a process locked a file and failed to
// ulock it before exiting.

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

bool lockfile_trylock(const char *name);
void lockfile_unlock(const char *name);

#ifdef __cplusplus
}
#endif

#endif
