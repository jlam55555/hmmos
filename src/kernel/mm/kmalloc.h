#pragma once

/// \file
/// \brief Very simple sequential allocator.
///
/// Temporary helper functions before we have a slab allocator. Never
/// frees memory. Maybe we can keep this around (or some version of a
/// sequential allocator w/ vmalloc, so the entire arena is contiguous
/// and can be freed at once) as a very simple allocator model.
///
/// TODO: rename this file to nonstd/new.h

#include <new>
#include <stddef.h>

namespace mem {

namespace phys {
class SimplePFA;
}
void set_pfa(phys::SimplePFA *pfa);

// These functions will return HHDM virtual addresses.
void *kmalloc(size_t sz) noexcept;
void kfree(void *data) noexcept;

} // namespace mem

// We don't have exceptions so this is actually equivalent to the
// nothrow variant. If using the `new` operator, we'll immediately
// page fault in this case due to attempting to run the constructor on
// the null page.
void *operator new(size_t sz);
void *operator new(size_t sz, const std::nothrow_t &tag) noexcept;
void operator delete(void *ptr) noexcept;
