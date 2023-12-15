#include "lf_bcast.h"
#include "test_header.h"

static bool pub(lf_bcast_t *b, u64 val)
{
  return lf_bcast_pub(b, &val, sizeof(val));
}

static bool sub_next(lf_bcast_sub_t *sub, u64 *val)
{
  size_t msg_sz;
  size_t drops;
  bool success = lf_bcast_sub_next(sub, val, &msg_sz, &drops);
  if (!success) return success;
  assert(msg_sz == sizeof(u64));
  return true;
}

static void test_simple(void)
{
  u64 val;

  lf_bcast_t *b = lf_bcast_new(2, sizeof(val));
  if (!b) FAIL("Failed to allocate bcast");

  REQUIRE(pub(b, 2));
  REQUIRE(pub(b, 3));

  lf_bcast_sub_t sub_1[1];
  lf_bcast_sub_begin(sub_1, b);
  REQUIRE(sub_next(sub_1, &val) && val == 2);
  REQUIRE(sub_next(sub_1, &val) && val == 3);
  REQUIRE(!sub_next(sub_1, &val));

  lf_bcast_sub_t sub_2[1];
  lf_bcast_sub_begin(sub_2, b);
  REQUIRE(sub_next(sub_2, &val) && val == 2);
  REQUIRE(sub_next(sub_2, &val) && val == 3);
  REQUIRE(!sub_next(sub_2, &val));

  REQUIRE(pub(b, 4));

  REQUIRE(sub_next(sub_1, &val) && val == 4);
  REQUIRE(!sub_next(sub_1, &val));
  REQUIRE(sub_next(sub_2, &val) && val == 4);
  REQUIRE(!sub_next(sub_2, &val));

  lf_bcast_sub_t sub_3[1];
  lf_bcast_sub_begin(sub_3, b);
  REQUIRE(sub_next(sub_3, &val) && val == 3);
  REQUIRE(sub_next(sub_3, &val) && val == 4);
  REQUIRE(!sub_next(sub_3, &val));

  lf_bcast_delete(b);
}

int main()
{
  test_simple();
  return 0;
}
