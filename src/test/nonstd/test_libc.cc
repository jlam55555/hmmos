#include "../test.h"
#include "nonstd/libc.h"

namespace {

/// Fills buffer with some byte pattern.
template <size_t N> void fill_buf(char (&buf)[N]) {
  for (int i = 0; i < N; ++i) {
    buf[i] = i;
  }
}

/// Compares two buffers, returns a signed result similar to `memcmp`.
template <size_t N> int compare_buf(char (&buf1)[N], char (&buf2)[N]) {
  for (int i = 0; i < N; ++i) {
    if (buf1[i] != buf2[i]) {
      return buf1[i] - buf2[i];
    }
  }
  return 0;
}

} // namespace

TEST(nonstd, memcpy) {
  char buf1[256] = {};
  char buf2[256] = {};
  fill_buf(buf1);
  TEST_ASSERT(compare_buf(buf1, buf2));
  memcpy(buf2, buf1, sizeof buf1);
  TEST_ASSERT(!compare_buf(buf1, buf2));
}

TEST(nonstd, memcmp) {
  // Equality.
  char buf1[256] = {};
  char buf2[256] = {};
  fill_buf(buf1);
  TEST_ASSERT(memcmp(buf1, buf2, sizeof buf1));
  memcpy(buf2, buf1, sizeof buf1);
  TEST_ASSERT(!memcmp(buf1, buf2, sizeof buf1));

  // Less than.
  buf1[0] = 'a';
  buf1[1] = 'b';
  buf1[2] = 'c';
  buf2[0] = 'a';
  buf2[1] = 'b';
  buf2[2] = 'd';
  TEST_ASSERT(memcmp(buf1, buf2, sizeof buf1) < 0);

  // Greater than.
  buf1[2] = 'e';
  TEST_ASSERT(memcmp(buf1, buf2, sizeof buf1) > 0);
}

TEST(nonstd, memmove) {
  // Test copying an overlapping region. One of these will fail using
  // memcpy().
  char buf1[256] = {};
  char buf2[256] = {};
  fill_buf(buf1);
  memcpy(buf2, buf1, sizeof buf1);
  TEST_ASSERT(memcmp(&buf1[32], &buf2[64], 128));
  memmove(&buf1[32], &buf1[64], 128);
  TEST_ASSERT(!memcmp(&buf1[32], &buf2[64], 128));

  fill_buf(buf1);
  memcpy(buf2, buf1, sizeof buf1);
  TEST_ASSERT(memcmp(&buf1[64], &buf2[32], 128));
  memmove(&buf1[64], &buf1[32], 128);
  TEST_ASSERT(!memcmp(&buf1[64], &buf2[32], 128));
}

TEST(nonstd, memset) {
  char buf1[256] = {};
  char buf2[256] = {};
  memset(buf1, 0x88, sizeof buf1);
  TEST_ASSERT(compare_buf(buf1, buf2));
  for (int i = 0; i < sizeof buf1; ++i) {
    buf2[i] = 0x88;
  }
  TEST_ASSERT(!compare_buf(buf1, buf2));
}

TEST(nonstd, isprint) {
  TEST_ASSERT(!isprint(0));
  TEST_ASSERT(!isprint(31));
  TEST_ASSERT(isprint(' '));
  for (int i = 0; i < 10; ++i) {
    TEST_ASSERT(isprint(i + '0'));
  }
  for (int i = 0; i < 26; ++i) {
    TEST_ASSERT(isprint(i + 'a'));
    TEST_ASSERT(isprint(i + 'A'));
  }
}

TEST(nonstd, strlen) {
  // This is a no-op test at runtime.
  static_assert(!strlen(""));
  static_assert(strlen("hello") == 5);
  static_assert(strlen("hel\x00lo") == 3);
  static_assert(strlen("hel\n\x12lo") == 7);
}

TEST(nonstd, strcmp) {
  static_assert(strcmp("abc", "abc") == 0);
  static_assert(strcmp("abd", "abc") > 0);
  static_assert(strcmp("abc", "abd") < 0);
  static_assert(strcmp("abcd", "abc") > 0);
  static_assert(strcmp("abc", "abcd") < 0);
}

TEST(nonstd, strncmp) {
  static_assert(strncmp("abc", "abc", 3) == 0);
  static_assert(strncmp("abd", "abc", 3) > 0);
  static_assert(strncmp("abc", "abd", 3) < 0);
  static_assert(strncmp("abcd", "abc", 3) == 0);
  static_assert(strncmp("abc", "abcd", 3) == 0);
}

TEST(nonstd, sprintf) {
  // TODO(jlam): I got lazy and it's 2AM. Unit test the sprintf
  // function.
}
