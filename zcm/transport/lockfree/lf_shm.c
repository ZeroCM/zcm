#include "lf_shm.h"
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>


bool lf_shm_create(const char *name, size_t size)
{
  int shm_fd = shm_open(name, O_CREAT|O_RDWR|O_EXCL, 0644);
  if (shm_fd < 0) return false;
  if (ftruncate(shm_fd, size) < 0) {
    close(shm_fd);
    return false;
  }
  close(shm_fd);
  return true;
}

void lf_shm_remove(const char *name)
{
  shm_unlink(name);
}

void * lf_shm_open(const char *name, size_t * _opt_size)
{
  int shm_fd = shm_open(name, O_CREAT|O_RDWR, 0755);
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
