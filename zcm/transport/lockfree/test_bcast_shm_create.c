#include "test_bcast_shm.h"

int main(int argc, char *argv[])
{
  if (argc != 2) {
    fprintf(stderr, "usage: %s <name>\n", argv[0]);
    return 1;
  }
  const char *name = argv[1];

  size_t size = region_size();
  if (!lf_shm_create(name, size)) {
    fprintf(stderr, "ERROR: Failed to create shm region '%s'\n", name);
    return 2;
  }

  void *mem = lf_shm_open(name, NULL);
  if (!mem) {
    fprintf(stderr, "ERROR: Failed to open shm region '%s'\n", name);
    return 3;
  }

  if (!lf_bcast_mem_init(mem, DEPTH, MAX_MSG_SZ)) {
    fprintf(stderr, "ERROR: Failed to init shm region '%s'\n", name);
    return 4;
  }

  lf_shm_close(mem, size);
  return 0;
}
