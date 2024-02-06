#pragma once
#include "lf_shm.h"
#include "lf_bcast.h"
#include "lf_util.h"
#include "test_header.h"
#include <unistd.h>

#define DEPTH      1024
#define MAX_MSG_SZ 1024

static inline size_t region_size(void)
{
  size_t size, align;
  lf_bcast_footprint(DEPTH, MAX_MSG_SZ, &size, &align);

  size_t pagesize = getpagesize();
  assert(LF_IS_POW2(pagesize));
  return LF_ALIGN_UP(size, pagesize);
}
