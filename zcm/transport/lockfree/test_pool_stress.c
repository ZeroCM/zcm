#include "lf_pool.h"
#include "test_header.h"

typedef struct thread_state thread_state_t;
struct thread_state
{
  pthread_t thread;
  lf_pool_t *pool;
  size_t n_attempts;
  size_t n_success;
  i64 dt;
};

static void * thread_func(void *usr)
{
  thread_state_t *t = (thread_state_t*)usr;

  lf_pool_t *p = t->pool;
  i64 start_time = wallclock();
  for (size_t i = 0; i < (size_t)1e5; i++) {
    void *elt = lf_pool_acquire(p);
    t->n_attempts++;
    if (elt) {
      t->n_success++;
      lf_pool_release(p, elt);
    }
  }
  t->dt = wallclock() - start_time;
  return NULL;
}

#define MAX_THREADS 128
static void run_test(const char *test_name, size_t num_threads, size_t num_elts, size_t elt_sz)
{
  lf_pool_t *p = lf_pool_new(num_elts, elt_sz);
  if (!p) FAIL("Failed to create new pool");

  assert(num_threads < MAX_THREADS);
  thread_state_t threads[MAX_THREADS];

  for (size_t i = 0; i < num_threads; i++) {
    thread_state_t *t = &threads[i];
    memset(t, 0, sizeof(*t));
    t->pool = p;
    pthread_create(&t->thread, NULL, thread_func, t);
  }
  for (size_t i = 0; i < num_threads; i++) {
    pthread_join(threads[i].thread, NULL);
  }

  lf_pool_delete(p);

  // Report stats:
  printf("Test: %s\n", test_name);
  for (size_t i = 0; i < num_threads; i++) {
    thread_state_t *t = &threads[i];
    printf("  Thread %zu | n_attempts: %zu n_success: %zu %.f nanos/attempt\n", i, t->n_attempts, t->n_success, (double)t->dt/t->n_attempts);
  }
}

int main()
{
  // Threads fighting over a single tiny element
  run_test("2thread_1elt", 2, 1, 1);
  run_test("8thread_1elt", 8, 1, 1);

  // Threads fighting over two medium-sized element
  run_test("2thread_2elt", 2, 2, 128);
  run_test("8thread_2elt", 8, 2, 128);

  // Plenty of elts
  run_test("2thread_2elt", 2, 64, 128);
  run_test("4thread_2elt", 4, 64, 128);
  run_test("5thread_2elt", 5, 64, 128);
  run_test("8thread_2elt", 8, 64, 128);

  return 0;
}
