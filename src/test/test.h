#pragma once

#include "nonstd/string_view.h"
#include "perf.h"

/// \file
/// \brief Simple C++-oriented test runner. Based off the laos test runner.
///
/// To define a new test, use the DEFINE_TEST macro:
///
///     DEFINE_TEST(foo::bar, testBaz) {
///         TEST_ASSERT(1 == 2);
///     }
///
/// Test descriptors are placed in their own special data section
/// for automatic discovery.
///
/// Tests are not built into the main kernel, but rather into a
/// separate test-runner kernel (which basically contains all the
/// kernel files except the entrypoint + test files + a custom test
/// runner entrypoint). Run this test-runner kernel using:
///
///     make run TEST=<selection_criteria>
///
/// Test selection follows a very simple pattern matcher:
///
/// - "all" matches all tests
/// - "<search>" matches strings containing <search>
/// - "^<prefix>" only matches tests beginning with <prefix>
/// - "<suffix>~" only matches tests ending with <suffix>
/// - "<pat1>,<pat2>" matches either <pat1> or <pat2>
///
/// Currently there is no syntax to exclude a test or to perform an
/// prefix/suffix operator on multiple subpatterns, because I'm lazy
/// and this is good enough for basic test selection.

namespace test {

/// \brief For use with TEST*_WITH_FIXTURE().
///
/// Usage instructions:
/// - Fixtures should (publically) subclass from this and define a
///   setup() method. It should _not_ override the run(bool&) method.
/// - Any state set up by the fixture should exist as public or
///   protected member variables, so they can be accessed by the test
///   function (which will run in a subclass).
///
class TestFixture {
public:
  virtual void setup() = 0;
  virtual void run(bool &) = 0;
};

/// \brief Run tests that match the test_selection criteria. For use
/// by the test-runner kernel entry point.
void run_tests(const char *test_selection);

namespace detail {

/// \brief Test descriptor.
///
struct TestInfo {
  const char *name;
  // This takes a bool& argument rather than returning a bool so that
  // test doesn't have to explicitly return true.
  void (*fn)(bool &success);

  // Sort tests by name.
  auto operator<=>(const TestInfo &rhs) const {
    return nonstd::string_view{name} <=> rhs.name;
  }
} __attribute__((packed));

/// \brief Test selection logic. Only exposed for unit testing.
///
bool matches(nonstd::string_view test_name, nonstd::string_view test_selection);

} // namespace detail

} // namespace test

// Helper functions for TEST*(). Don't use directly.
#define _DEFINE_TEST(TEST_NS, TEST_NAME_SYM, TEST_NAME_STR)                    \
  namespace TEST_NS::test {                                                    \
  void TEST_NAME_SYM(bool &); /* fwd decl */                                   \
  }                                                                            \
  namespace {                                                                  \
  __attribute__((                                                              \
      section("data_test_info"),                                               \
      used)) volatile test::detail::TestInfo test_info_##TEST_NAME_SYM{        \
      .name = #TEST_NS "::" #TEST_NAME_STR,                                    \
      .fn = &TEST_NS::test::TEST_NAME_SYM,                                     \
  };                                                                           \
  }                                                                            \
  void TEST_NS::test::TEST_NAME_SYM(bool &_test_passed)

#define _DEFINE_TEST_WITH_FIXTURE(TEST_NS, TEST_NAME_SYM, TEST_NAME_STR,       \
                                  FIXTURE)                                     \
  namespace TEST_NS::test {                                                    \
  class Fixture##TEST_NAME_SYM final : public FIXTURE {                        \
    static_assert(std::derived_from<FIXTURE, ::test::TestFixture>);            \
    void run(bool &) final;                                                    \
  };                                                                           \
  void TEST_NAME_SYM(bool &res) {                                              \
    ::test::TestFixture &&fixture = Fixture##TEST_NAME_SYM{};                  \
    fixture.setup();                                                           \
    fixture.run(res);                                                          \
  }                                                                            \
  }                                                                            \
  namespace {                                                                  \
  __attribute__((                                                              \
      section("data_test_info"),                                               \
      used)) volatile test::detail::TestInfo test_info_##TEST_NAME_SYM{        \
      .name = #TEST_NS "::" #TEST_NAME_STR,                                    \
      .fn = &TEST_NS::test::TEST_NAME_SYM,                                     \
  };                                                                           \
  }                                                                            \
  void TEST_NS::test::Fixture##TEST_NAME_SYM::run(bool &_test_passed)

/// \brief Define a test.
///
/// A namespace must be provided, and this macro must be placed in the
/// global namespace.
///
/// Note that tests are defined in (a sub-namespace of) TEST_NS, so
/// all symbols from TEST_NS are automatically available to the test
/// function.
#define TEST(TEST_NS, TEST_NAME) _DEFINE_TEST(TEST_NS, _##TEST_NAME, TEST_NAME)

/// \brief Define a test for a class.
///
/// Similar to DEFINE_TEST, where the test will be placed in TEST_NS,
/// except that CLASS_NAME will be added to the test name.
#define TEST_CLASS(TEST_NS, CLASS_NAME, TEST_NAME)                             \
  _DEFINE_TEST(TEST_NS, _##TEST_NAME, CLASS_NAME::TEST_NAME)

#define TEST_WITH_FIXTURE(TEST_NS, TEST_NAME, FIXTURE)                         \
  _DEFINE_TEST_WITH_FIXTURE(TEST_NS, _##TEST_NAME, TEST_NAME, FIXTURE)
#define TEST_CLASS_WITH_FIXTURE(TEST_NS, CLASS_NAME, TEST_NAME, FIXTURE)       \
  _DEFINE_TEST_WITH_FIXTURE(TEST_NS, _##TEST_NAME, CLASS_NAME::TEST_NAME,      \
                            FIXTURE)

/// \brief Run an assertion within a test.
///
/// TODO: make this work even in nested functions -- we'll need
/// something like setjmp/longjmp for this non-local goto. For now
/// we're restricted to assertions only in the top-level test
/// function.
#define TEST_ASSERT(...)                                                       \
  if (unlikely(!(__VA_ARGS__))) {                                              \
    nonstd::printf("ASSERTION FAILED (%s:%u): %s\r\n", __FILE__, __LINE__,     \
                   #__VA_ARGS__);                                              \
    _test_passed = false;                                                      \
    return;                                                                    \
  }
