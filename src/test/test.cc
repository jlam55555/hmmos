#include "test.h"
#include "nonstd/libc.h"
#include "nonstd/string_view.h"
#include <algorithm>

extern test::detail::TestInfo __start_data_test_info;
extern test::detail::TestInfo __stop_data_test_info;

namespace test {

bool detail::matches(nonstd::string_view test_name,
                     nonstd::string_view test_selection) {
  // Special keyword "all" acts the same as "" (matches everything).
  if (test_selection == "all") {
    return true;
  }

  // Break up into disjunctions.
  if (size_t comma_pos = test_selection.find(',');
      comma_pos != nonstd::string_view::npos) {
    return matches(test_name, test_selection.substr(0, comma_pos)) ||
           matches(test_name, test_selection.substr(comma_pos + 1));
  }

  // Check ^/~ checks.
  bool starts_with = false;
  bool ends_with = false;
  if (test_selection.starts_with('^')) {
    test_selection = test_selection.substr(1);
    starts_with = true;
  }
  if (test_selection.ends_with('~')) {
    test_selection = test_selection.substr(0, test_selection.size() - 1);
    ends_with = true;
  }
  if ((starts_with && !test_name.starts_with(test_selection)) ||
      (ends_with && !test_name.ends_with(test_selection))) {
    return false;
  }

  // Regular matching.
  return test_name.contains(test_selection);
}

bool run(const detail::TestInfo *test) {
  bool res = true;
  test->fn(res);
  nonstd::printf("TEST RESULT %s: %d\r\n", test->name, res);
  return res;
}

void run_tests(const char *test_selection) {
  nonstd::printf("TEST SELECTION=%s\r\n", test_selection);

  std::sort(&__start_data_test_info, &__stop_data_test_info);

  unsigned tests_run = 0, tests_passed = 0;
  for (const detail::TestInfo *test_info = &__start_data_test_info;
       test_info != &__stop_data_test_info; ++test_info) {
    if (detail::matches(test_info->name, test_selection)) {
      ++tests_run;
      tests_passed += run(test_info);
    }
  }

  nonstd::printf("SUMMARY: %u/%u tests passed\r\n", tests_passed, tests_run);
}

} // namespace test
