#include "test_bcast_shm.h"
#include <inttypes.h>

#define MAX_RECV 128

typedef struct recv_state recv_state_t;
struct recv_state
{
  u32  cnt_start;
  u32  cnt_end;
};

static void report_stats(recv_state_t *recv, size_t n)
{
  printf("STATS:\n");
  for (size_t i = 0; i < n; i++) {
    recv_state_t *r = &recv[i];
    if (!r->cnt_start) continue;
    printf("  %5zu | [%u, %u] (%u)\n", i, r->cnt_start, r->cnt_end, r->cnt_end - r->cnt_start);
    r->cnt_start = 0;
  }
}

static void exec_operation_pub(lf_bcast_t *b, u32 id)
{
  u64 count = (u64)id << 32;

  i64 dt_acc = 0;
  size_t dt_count = 0;

  while (1) {
    count++;
    i64 start_tm = wallclock();
    bool success = lf_bcast_pub(b, &count, sizeof(count));
    dt_acc += wallclock() - start_tm;
    dt_count++;
    if (!success) printf("WARN: Failed to publish!\n");
    if (dt_count == 10000) {
      printf("Stats: %lu nanos / pub\n", (long)(dt_acc/dt_count));
      dt_acc = 0;
      dt_count = 0;
    }
    //usleep(1e6); //100);
    usleep(100);
  }
}

static void exec_operation_sub(lf_bcast_t *b)
{
  lf_bcast_sub_t sub[1];
  lf_bcast_sub_begin(sub, b);

  recv_state_t recv[MAX_RECV] = {};

  char msg[MAX_MSG_SZ];
  size_t msg_sz;
  size_t drops;

  i64 start_tm = wallclock();
  while (1) {
    i64 now = wallclock();
    if (now - start_tm > (i64)1e9) {
      report_stats(recv, ARRAY_SIZE(recv));
      start_tm = now;
    }

    if (!lf_bcast_sub_next(sub, msg, &msg_sz, &drops)) continue;
    assert(msg_sz == sizeof(u64));

    u32 id, cnt;
    memcpy(&cnt, msg, sizeof(u32));
    memcpy(&id, msg+sizeof(u32), sizeof(u32));

    if (id < ARRAY_SIZE(recv)) {
      recv_state_t *r = &recv[id];
      if (!r->cnt_start) r->cnt_start = cnt;
      r->cnt_end = cnt;
    }
  }
}

int main(int argc, char *argv[])
{
  if (argc < 3) {
    fprintf(stderr, "usage: %s <name> <oper> [<id>]\n", argv[0]);
    return 1;
  }
  const char *name = argv[1];
  const char *oper = argv[2];
  u32         id   = argc > 3 ? atoi(argv[3]) : 0;

  size_t size = 0;
  void *mem = lf_shm_open(name, &size);
  if (!mem) {
    fprintf(stderr, "ERROR: Failed to open shm region '%s'\n", name);
    return 2;
  }

  size_t exp_size = region_size();
  if (size != exp_size) {
    fprintf(stderr, "ERROR: Region size mismatches expected size\n");
    return 3;
  }

  lf_bcast_t * b = lf_bcast_mem_join(mem, DEPTH, MAX_MSG_SZ);
  if (!b) {
    fprintf(stderr, "ERROR: Failed to join shm region '%s'\n", name);
    return 4;
  }

  if (0) {}
  else if (0 == strcmp(oper, "pub")) exec_operation_pub(b, id);
  else if (0 == strcmp(oper, "sub")) exec_operation_sub(b);
  else {
    fprintf(stderr, "ERROR: Unknown operation: '%s'\n", oper);
    return 5;
  }

  lf_bcast_mem_leave(b);
  lf_shm_close(mem, size);
  return 0;
}
