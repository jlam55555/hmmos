#include "test.h"

TEST(test, always_succeed) { TEST_ASSERT(true); }

TEST(test, always_fail) { TEST_ASSERT(false); }

TEST(test, matches_all) {
  TEST_ASSERT(detail::matches("test::matches_all", "all"));
  TEST_ASSERT(detail::matches("foo::bar", "all"));
  TEST_ASSERT(detail::matches("foo::baz", "all"));

  // "" also matches all since it's a substring of every string.
  TEST_ASSERT(detail::matches("test::matches_all", ""));
  TEST_ASSERT(detail::matches("foo::bar", ""));
  TEST_ASSERT(detail::matches("foo::baz", ""));
}

TEST(test, substring) {
  TEST_ASSERT(detail::matches("foobar", "foobar"));
  TEST_ASSERT(!detail::matches("foo", "foobar"));
  TEST_ASSERT(detail::matches("foobar", "foo"));
}

TEST(test, disjunction) {
  TEST_ASSERT(detail::matches("test1", "test1,test2"));
  TEST_ASSERT(detail::matches("test2", "test1,test2"));
  TEST_ASSERT(detail::matches("hellotest1", "test1,test2"));
  TEST_ASSERT(detail::matches("test2wo::rld", "test1,test2"));
  TEST_ASSERT(!detail::matches(",", "test1,test2"));
  TEST_ASSERT(!detail::matches("1,te", "test1,test2"));
}

TEST(test, prefix) {
  TEST_ASSERT(detail::matches("hello", "^hello"));
  TEST_ASSERT(detail::matches("hello::world", "^hello"));
  TEST_ASSERT(!detail::matches("world::hello", "^hello"));
}

TEST(test, suffix) {
  TEST_ASSERT(detail::matches("hello", "hello~"));
  TEST_ASSERT(!detail::matches("hello::world", "hello~"));
  TEST_ASSERT(detail::matches("world::hello", "hello~"));
}

TEST(test, symbols) {
  // Any characters outside the spec (^, ~, ',', null) are
  // allowed. Technically. Even if they don't really work due to C++
  // symbol naming restrictions.
  TEST_ASSERT(detail::matches("!@#$%&*()_+", "!@#$%&*()_+"));

  // These don't work because these characters are special.
  TEST_ASSERT(!detail::matches("^foo", "^foo"));
  TEST_ASSERT(!detail::matches("foo~", "foo~"));
}

TEST(test, complex) {
  TEST_ASSERT(detail::matches("test::complex", "test"));
  TEST_ASSERT(detail::matches("test::complex", "t::c"));
  TEST_ASSERT(detail::matches("test::complex", "complex"));
  TEST_ASSERT(detail::matches("test::complex", "^test::"));
  TEST_ASSERT(detail::matches("test::complex", "^test::,^libc"));
  TEST_ASSERT(detail::matches("test::complex", "^test::complex~"));
  TEST_ASSERT(!detail::matches("test::complex", "a,b"));
  TEST_ASSERT(detail::matches("test::complex", "a,b,c"));
  TEST_ASSERT(!detail::matches("test::complex", "complex::test"));
}
