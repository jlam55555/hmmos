#include "../test.h"
#include "./leak_checker.h"
#include "nonstd/string.h"
#include <type_traits>

TEST_CLASS_WITH_FIXTURE(nonstd, string, basic, LeakChecker) {
  const string s1{"foobar"};
  const char *s2 = "foobar";
  TEST_ASSERT(s1.length() == strlen(s2));
  for (size_t i = 0; i < s1.length(); ++i) {
    TEST_ASSERT(s1[i] == s2[i]);
  }
  TEST_ASSERT(s1[s1.length()] == '\0');
  TEST_ASSERT(!strcmp(s1.data(), s2));
}

TEST_CLASS_WITH_FIXTURE(nonstd, string, constructor, LeakChecker) {
  const char *input_it_source = "foobar";
  const string s{"baz"};

  TEST_ASSERT(string{} == "");
  TEST_ASSERT(string{"foo"} == "foo");
  TEST_ASSERT(string{10, 'f'} == "ffffffffff");
  TEST_ASSERT(string{input_it_source, input_it_source + 5} == "fooba");
  TEST_ASSERT(string{input_it_source} == "foobar");
  TEST_ASSERT(string{input_it_source, 4} == "foob");
  TEST_ASSERT(string{string_view{input_it_source}} == "foobar");
  TEST_ASSERT(string{s} == s);
  TEST_ASSERT(string{string{"foobarbaz"}} == "foobarbaz");
}

TEST_CLASS_WITH_FIXTURE(nonstd, string, assignment, LeakChecker) {
  string s;
  string s2{"foo"};
  const char *s3 = "baz";
  constexpr string_view s4 = "bar";
  s = s2;
  TEST_ASSERT(s == s2);
  s = s3;
  TEST_ASSERT(s == s3);
  s = s4;
  TEST_ASSERT(s == s4);
}

TEST_CLASS_WITH_FIXTURE(nonstd, string, element_access, LeakChecker) {
  const string s = "foobar";
  TEST_ASSERT(s[0] == 'f');
  TEST_ASSERT(s.at(0) == 'f');
  TEST_ASSERT(s[1] == 'o');
  TEST_ASSERT(s.at(1) == 'o');
  TEST_ASSERT(s[s.length()] == '\0');
  // at() has bounds-checking, can't do s.at(s.length())

  string mutable_s = "bar";
  mutable_s[2] = 'z';
  mutable_s[1] = 'r';
  mutable_s.front() = 'a';
  TEST_ASSERT(mutable_s == "arz");
}

TEST_CLASS_WITH_FIXTURE(nonstd, string, iterators, LeakChecker) {
  const string s = "helloworld";
  TEST_ASSERT(string{s.begin(), s.end()} == s);
  TEST_ASSERT(string{s.rbegin(), s.rend()} == "dlrowolleh");

  TEST_ASSERT(*s.begin() == s.front());
  TEST_ASSERT(*std::prev(s.end()) == s.back());
  TEST_ASSERT(*s.rbegin() == s.back());
  TEST_ASSERT(*std::prev(s.rend()) == s.front());
}

TEST_CLASS_WITH_FIXTURE(nonstd, string, capacity, LeakChecker) {
  const string empty;
  TEST_ASSERT(empty.empty());
  TEST_ASSERT(empty.size() == 0);
  TEST_ASSERT(empty.length() == empty.size());
  TEST_ASSERT(empty.capacity() >= empty.length());

  const string nonempty{"foo"};
  TEST_ASSERT(!nonempty.empty());
  TEST_ASSERT(nonempty.size() == 3);
  TEST_ASSERT(nonempty.length() == nonempty.size());
  TEST_ASSERT(nonempty.capacity() >= nonempty.length());

  // reserve() should not change string contents. The capacity cannot
  // decrease smaller than the string length. Any requests for a
  // smaller capacity are non-binding, i.e. the end capacity is only
  // guaranteed to be >= the requested capacity.
  string s = "foo";
  TEST_ASSERT(s == "foo");
  TEST_ASSERT(s.length() == 3);
  TEST_ASSERT(s.capacity() >= s.length());
  size_t requested_capacity = 100;
  s.reserve(requested_capacity);
  TEST_ASSERT(s == "foo");
  TEST_ASSERT(s.length() == 3);
  TEST_ASSERT(s.capacity() >= requested_capacity);
  requested_capacity = 1;
  s.reserve(requested_capacity);
  TEST_ASSERT(s == "foo");
  TEST_ASSERT(s.length() == 3);
  TEST_ASSERT(s.capacity() >= requested_capacity);
}

TEST_CLASS_WITH_FIXTURE(nonstd, string, modifiers, LeakChecker) {
  string s = "foo";
  TEST_ASSERT(s.length() == 3);
  s.clear();
  TEST_ASSERT(s == "");

  s = "foo";
  s.push_back('b');
  TEST_ASSERT(s == "foob");

  s.append(10, 'f');
  TEST_ASSERT(s == "foobffffffffff");

  s.append("apple");
  TEST_ASSERT(s == "foobffffffffffapple");

  constexpr string_view s2 = "barbaz";
  s = "foo";
  s.append(s2.begin(), s2.end());
  TEST_ASSERT(s == "foobarbaz");

  s.append({'a', 'b', 'c'});
  TEST_ASSERT(s == "foobarbazabc");

  s = "foo";
  TEST_ASSERT(s.length() == 3);
  TEST_ASSERT(s == "foo");
  s.resize(10);
  TEST_ASSERT(s.length() == 10);
  // Resize zero-extends but length information is maintained.
  TEST_ASSERT(!strcmp(s.data(), "foo"));
  TEST_ASSERT(s != "foo");
  TEST_ASSERT(s == string_view{"foo\x00\x00\x00\x00\x00\x00\x00", 10});
  TEST_ASSERT(string_view{s}.length() == 10);
  // If we overwrite the zero-extended characters, they appear in the
  // string.
  memcpy(s.data() + 3, "barbazf", 7);
  TEST_ASSERT(s.length() == 10);
  TEST_ASSERT(s == "foobarbazf");
  s.resize(1);
  TEST_ASSERT(s.length() == 1);
  TEST_ASSERT(s == "f");
}

TEST_CLASS_WITH_FIXTURE(nonstd, string, non_member_functions, LeakChecker) {
  TEST_ASSERT(string{"foo"} + string{"bar"} == "foobar");
  TEST_ASSERT(string{"foo"} + "bar" == "foobar");
  TEST_ASSERT(string{"foo"} + 'b' == "foob");
  TEST_ASSERT(string{"foo"} + "bar"_sv == "foobar");
  TEST_ASSERT("foo" + string{"bar"} == "foobar");
  TEST_ASSERT('f' + string("oob") == "foob");
  TEST_ASSERT("foo"_sv + string{"bar"} == "foobar");

  TEST_ASSERT(string{"foo"} == string{"foo"});
  TEST_ASSERT(string{"foo"} != string{"bar"});
  TEST_ASSERT(string{"foo"} == "foo");
  TEST_ASSERT(string{"foo"} != "bar");
  TEST_ASSERT("foo" == string{"foo"});
  TEST_ASSERT("foo" != string{"bar"});

  // Comparisons to string_view use string_view's comparison operator
  // after implicit conversion to string_view.
  TEST_ASSERT(string{"foo"} == "foo"_sv);
  TEST_ASSERT(string{"foo"} != "bar"_sv);
  TEST_ASSERT("foo"_sv == string{"foo"});
  TEST_ASSERT("foo"_sv != string{"bar"});
}
