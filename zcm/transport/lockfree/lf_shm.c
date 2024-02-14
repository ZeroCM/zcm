#include "lf_shm.h"
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <libgen.h>

// like mkdir -p but skips the trailing component
static void mkdir_with_parents(const char *path, mode_t mode)
{
    int len = strlen(path);
    for (int i = 0; i < len; i++) {
        if (path[i]=='/') {
            char *dirpath = (char *) malloc(i+1);
            strncpy(dirpath, path, i);
            dirpath[i]=0;

            mkdir(dirpath, mode);
            free(dirpath);

            i++; // skip the '/'
        }
    }
}

bool lf_shm_create(const char *path, size_t size)
{
    mkdir_with_parents(path, 0775);

    int shm_fd = open(path, O_CREAT|O_EXCL|O_RDWR|O_CLOEXEC, 0644);
    if (shm_fd < 0) return false;

    if (ftruncate(shm_fd, size) < 0) {
        close(shm_fd);
        return false;
    }
    close(shm_fd);
    return true;
}

void lf_shm_remove(const char *path)
{
    unlink(path);
}

void * lf_shm_open(const char *path, size_t * _opt_size)
{
    int shm_fd = open(path, O_RDWR|O_CLOEXEC);
    if (shm_fd < 0) return NULL;

    struct stat st[1];
    if (0 != fstat(shm_fd, st)) return NULL;
    size_t size = st->st_size;

    void *mem = mmap(NULL, size, PROT_READ|PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (mem == MAP_FAILED) return NULL;

    close(shm_fd);
    if (_opt_size) *_opt_size = size;
    return mem;
}

void lf_shm_close(void *shm, size_t size)
{
    munmap(shm, size);
}
