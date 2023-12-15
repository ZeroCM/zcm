#include <assert.h>
#include <limits.h>
#include <stdlib.h>

// If we're compiling in an older C std, we might not have static_assert()
#ifndef static_assert
# define static_assert(cond, _) typedef int static_assert_ ## __LINE__[(cond) ? 1 : -1]
#endif

// Machine is byte-addressable
static_assert(CHAR_BIT == 8, "");

// Sizes / Offsets / Alignments are 64-bit
static_assert(sizeof(size_t) == 8, "");

// Assuming we have 64-bit pointers
static_assert(sizeof(void*) == 8, "");

// Long and Long Long are the same and 64-bits
static_assert(sizeof(long) == sizeof(long long), "");
static_assert(sizeof(long) == 8, "");

// Machine is little-endian
#if !defined(__BYTE_ORDER) || __BYTE_ORDER != __LITTLE_ENDIAN
# error "Machine is not little-endian"
#endif
