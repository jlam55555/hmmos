#pragma once

/// \file
/// \brief Allocator for (4KB) physical page frames.
///
/// Page frame allocators provide a simple `alloc()` and `free()`
/// interface for physical pages. Each PFA is responsible for some
/// contiguous range of memory -- it will determine which pages are
/// usable by looking at the PFT's usable regions.
///
/// SimplePFA is a very simple round-robin, first-fit allocator that
/// uses the PFT to determine if a page is free or not. This is pretty
/// terrible (first-fit means poor larger allocations, and using a
/// sparse table is bad cache-wise).

#include "mm/page_frame_table.h"

namespace mem::phys {

/// \brief Abstract base class for page frame allocators.
///
class PageFrameAllocator : public util::IntrusiveListHead<PageFrameAllocator> {
public:
  /// Construct an allocator which is responsible for the page frames
  /// with offsets [_start, end).
  PageFrameAllocator(PageFrameTable &pft, uint64_t _start, uint64_t _end);
  ~PageFrameAllocator();

  NON_MOVABLE(PageFrameAllocator);

  unsigned get_total_pages() const { return total_pgs; }
  unsigned get_free_pages() const {
    return get_total_pages() - get_alloced_pages();
  };
  virtual unsigned get_alloced_pages() const = 0;

  const uint64_t start;
  const uint64_t end;

protected:
  PageFrameTable &pft;

  PageFrameDescriptor *const startp = &pft.get_pfd(start);
  PageFrameDescriptor *const endp = &pft.get_pfd(end);

  std::span<const e820_mm_entry> get_usable_regions() const {
    return pft.get_usable_regions();
  }

private:
  // These are part of the interface, but we should really only be
  // calling these through the derived class interface to avoid
  // dynamic dispatch.
  virtual std::optional<uint64_t> alloc(unsigned num_pg) = 0;
  virtual void free(uint64_t base, uint64_t num_pg) = 0;

  unsigned total_pgs;
};

/// \brief Simple round-robin allocator that doesn't use auxiliary
/// memory
///
/// This keeps track of used pages via the PFT and uses a simple
/// round-robin linear scan to find free memory regions.
///
/// N.B. The needle does _not_ get incremented after allocation. This
/// means that in the case of an alloc-free-alloc pattern, the second
/// alloc will return the same base address as the first alloc if the
/// second alloc is of no greater size than the first.
class SimplePFA final : public PageFrameAllocator {
public:
  SimplePFA(PageFrameTable &pft, uint64_t _start, uint64_t _end);

  std::optional<uint64_t> alloc(unsigned num_pg) final;
  void free(uint64_t base, uint64_t num_pg) final;

  unsigned get_alloced_pages() const final { return alloced_pgs; }

private:
  PageFrameDescriptor *needle = &pft.get_pfd(start);
  unsigned alloced_pgs = 0;
};

} // namespace mem::phys
