/// Testing C/C++ runtime features

#include "../test.h"

namespace {

int b_init() { return 3; }

int a;
int b = b_init();

struct Bar {
  Bar(int _c) : c{_c} {}

  int c;
  int baz = 5;
} bar{4};

__attribute__((constructor)) void foo() { a = 2; }

} // namespace

TEST(nonstd::cpp, global_ctor) {
  TEST_ASSERT(a == 2);
  TEST_ASSERT(b == 3);
  TEST_ASSERT(bar.c == 4);
  TEST_ASSERT(bar.baz == 5);
}
