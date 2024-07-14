#include "../test.h"
#include "nonstd/string_view.h"

TEST_CLASS(nonstd, string_view, constexpr) {
  // No need to really define a test but might as well. This should
  // test most of the behavior we want since it all works with
  // constexpr.
  constexpr auto sv = "foo::bar"_sv;
  static_assert(sv == "foo::bar");
  static_assert(sv != "foo::baz");

  static_assert(!""_sv.size());
  static_assert("hello"_sv.size() == 5);
  static_assert("hello"_sv != "world");

  static_assert("hello"_sv.find("ello") != string_view::npos);
  static_assert("hello"_sv.find("olle") == string_view::npos);
  static_assert("hello"_sv.find('e') != string_view::npos);
  static_assert("hello"_sv.find('b') == string_view::npos);

  static_assert("hello"_sv.starts_with("hello"));
  static_assert("hello"_sv.starts_with(""));
  static_assert("hello"_sv.starts_with("h"));
  static_assert(!"hello"_sv.starts_with("helloworld"));
  static_assert(!"hello"_sv.starts_with("abc"));
  static_assert("hello"_sv.starts_with('h'));
  static_assert(!"hello"_sv.starts_with('o'));

  static_assert("hello"_sv.ends_with("hello"));
  static_assert("hello"_sv.ends_with(""));
  static_assert("hello"_sv.ends_with("o"));
  static_assert(!"hello"_sv.ends_with("helloworld"));
  static_assert(!"hello"_sv.ends_with("abc"));
  static_assert(!"hello"_sv.ends_with('h'));
  static_assert("hello"_sv.ends_with('o'));

  static_assert("helloworld"_sv.contains(""));
  static_assert("helloworld"_sv.contains("hello"));
  static_assert("helloworld"_sv.contains("world"));
  static_assert(!"helloworld"_sv.contains("worldhello"));
  static_assert(!"hello"_sv.contains("helloworld"));
}

TEST_CLASS(nonstd, string_view, substr) {
  // Again this is constexpr so no need for a real test.  The behavior
  // here is different than the standard, so I isolated it to its own
  // test.
  static_assert("hello"_sv.substr(0) == "hello"_sv);
  static_assert("hello"_sv.substr(1) == "ello"_sv);
  static_assert("hello"_sv.substr(5) == ""_sv);

  static_assert("hello"_sv.substr(0, string_view::npos) == "hello"_sv);
  static_assert("hello"_sv.substr(1, string_view::npos) == "ello"_sv);
  static_assert("hello"_sv.substr(5, string_view::npos) == ""_sv);

  static_assert("hello"_sv.substr(0, 2) == "he"_sv);
  static_assert("hello"_sv.substr(1, 2) == "el"_sv);
  static_assert("hello"_sv.substr(5, 0) == ""_sv);

  // This differs from the standard. We have bounds check, the
  // standard will throw.
  static_assert("hello"_sv.substr(2, 4) == "llo"_sv);
  static_assert("hello"_sv.substr(6) == ""_sv);
  static_assert("hello"_sv.substr(6, 3) == ""_sv);
  static_assert("hello"_sv.substr(6, string_view::npos) == ""_sv);
  static_assert("hello"_sv.substr(-1) == "hello"_sv);
  static_assert("hello"_sv.substr(-1, 3) == "hel"_sv);
  static_assert("hello"_sv.substr(-1, 6) == "hello"_sv);
  static_assert("hello"_sv.substr(-1, string_view::npos) == "hello"_sv);
}

TEST_CLASS(nonstd, string_view, non_constexpr) {
  char const *str1 = "hello";
  char const str2[] = "world!";

  TEST_ASSERT(string_view{str1} == "hello");
  TEST_ASSERT(string_view{str1}.length() == 5);

  TEST_ASSERT(string_view{str2} == "world!");
  TEST_ASSERT(string_view{str2}.length() == 6);

  TEST_ASSERT(string_view{str1, 3} == "hel");
}
