/// \file
/// \brief Symbols in the `std` namespace required by `std`
/// interfaces. If they have a trivial implementation, they can be
/// implemented here and we can use them instead of implementing a
/// `nonstd` version.

#include "nonstd/libc.h"
#include <cassert>
#include <new>

namespace std {
// std::function
void __throw_bad_function_call() {
  nonstd::printf("ASSERT FAILED: %s", __PRETTY_FUNCTION__);
  assert(false);
}
} // namespace std
