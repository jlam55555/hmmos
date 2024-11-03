/// \file
/// \brief Symbols in the `std` namespace required by `std`
/// interfaces. If they have a trivial implementation, they can be
/// implemented here and we can use them instead of implementing a
/// `nonstd` version.

#include <new>

////////////////////////////////////////////////////////////////////////////////
// new.h
////////////////////////////////////////////////////////////////////////////////
namespace std {
const std::nothrow_t nothrow{};
}
