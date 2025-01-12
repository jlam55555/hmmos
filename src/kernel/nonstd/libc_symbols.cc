/// \file
/// \brief Symbols in the `std` namespace required by `std`
/// interfaces. If they have a trivial implementation, they can be
/// implemented here and we can use them instead of implementing a
/// `nonstd` version.

#include "nonstd/libc.h"
#include <cassert>
#include <new>

////////////////////////////////////////////////////////////////////////////////
// new.h
////////////////////////////////////////////////////////////////////////////////
namespace std {
// ::operator new()
const std::nothrow_t nothrow{};
// std::function
void __throw_bad_function_call() {
  nonstd::printf("ASSERT FAILED: %s", __PRETTY_FUNCTION__);
  assert(false);
}
// std::string
void __throw_length_error(char const *err) {
  nonstd::printf("ASSERT FAILED: %s: %s", __PRETTY_FUNCTION__, err);
  assert(false);
}
void __throw_logic_error(char const *err) {
  nonstd::printf("ASSERT FAILED: %s: %s", __PRETTY_FUNCTION__, err);
  assert(false);
}
} // namespace std
