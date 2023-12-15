#include "lf_queue.h"
#include "test_header.h"

static void test_simple(void)
{
  lf_queue_t *q = lf_queue_new(1);
  if (!q) FAIL("Failed to allocate queue");
  u64 val;

  REQUIRE(lf_queue_enqueue(q, 45));
  REQUIRE(!lf_queue_enqueue(q, 65));
  REQUIRE(lf_queue_dequeue(q, &val) && val == 45);
  REQUIRE(!lf_queue_dequeue(q, &val));

  lf_queue_delete(q);
}

static void test_roll_over(void)
{
  lf_queue_t *q = lf_queue_new(2);
  if (!q) FAIL("Failed to allocate queue");
  u64 val;

  REQUIRE(lf_queue_enqueue(q, 45));
  REQUIRE(lf_queue_dequeue(q, &val) && val == 45);

  REQUIRE(lf_queue_enqueue(q, 65));
  REQUIRE(lf_queue_enqueue(q, 70));
  REQUIRE(!lf_queue_enqueue(q, 80));

  REQUIRE(lf_queue_dequeue(q, &val) && val == 65);
  REQUIRE(lf_queue_dequeue(q, &val) && val == 70);
  REQUIRE(!lf_queue_dequeue(q, &val));

  lf_queue_delete(q);
}

int main()
{
  test_simple();
  test_roll_over();
  return 0;
}
