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

void * lf_shm_open(const char *path, int flags, size_t * _opt_size, int *_opt_err)
{
    int shm_fd = open(path, O_RDWR|O_CLOEXEC);
    if (shm_fd < 0) {
        if (_opt_err) *_opt_err = LF_SHM_ERR_OPEN;
        return NULL;
    }

    struct stat st[1];
    if (0 != fstat(shm_fd, st)) {
        close(shm_fd);
        if (_opt_err) *_opt_err = LF_SHM_ERR_STAT;
        return NULL;
    }
    size_t size = st->st_size;

    char *mem = (char*)mmap(NULL, size, PROT_READ|PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (mem == MAP_FAILED) {
        close(shm_fd);
        if (_opt_err) *_opt_err = LF_SHM_ERR_MMAP;
        return NULL;
    }

    // prefault the entire region
    size_t pagesize = getpagesize();
    for (char *ptr = mem; ptr < mem+size; ptr += pagesize) {
        volatile char byte = *(volatile char *)ptr;
        (void)byte;
    }

    // mlock it if required
    if (flags & LF_SHM_FLAG_MLOCK) {
        int ret = mlock(mem, size);
        if (ret != 0) {
            printf("MLCOK FAILED: %d (%s)\n", errno, strerror(errno));
            munmap(mem, size);
            close(shm_fd);
            if (_opt_err) *_opt_err = LF_SHM_ERR_MLOCK;
            return NULL;
        }
    }

    close(shm_fd);
    if (_opt_size) *_opt_size = size;
    if (_opt_err) *_opt_err = LF_SHM_SUCCESS;
    return mem;
}

void lf_shm_close(void *shm, size_t size)
{
    munmap(shm, size);
}

const char *lf_shm_errstr(int err)
{
    switch (err) {
        case LF_SHM_SUCCESS: return "LF_SHM_SUCCESS";
        case LF_SHM_ERR_OPEN: return "LF_SHM_ERR_OPEN";
        case LF_SHM_ERR_STAT: return "LF_SHM_ERR_STAT";
        case LF_SHM_ERR_MMAP: return "LF_SHM_ERR_MMAP";
        case LF_SHM_ERR_SIZE: return "LF_SHM_ERR_SIZE";
        case LF_SHM_ERR_MLOCK: return "LF_SHM_ERR_MLOCK";
        default: return "LF_SHM_ERR_UNKNOWN";
    }
}
