#include "page_frame_allocator.h"

#include "perf.h"
#include "util/algorithm.h"

namespace mem::phys {

PageFrameAllocator::PageFrameAllocator(PageFrameTable &pft, uint64_t _start,
                                       uint64_t _end)
    : pft{pft}, start{_start}, end{_end} {
  ASSERT(start < end);
  ASSERT(pft.register_allocator(*this, start, end));

  // Precompute total pages.
  total_pgs = 0;
  for (auto region : get_usable_regions()) {
    uint64_t overlap_start = std::max(region.base, start);
    uint64_t overlap_end = std::min(region.base + region.len, end);
    if (overlap_end > overlap_start) {
      total_pgs += overlap_end - overlap_start;
    }
  }
  total_pgs >>= PG_SZ_BITS;
}

PageFrameAllocator::~PageFrameAllocator() {
  // Note that there's nothing to prevent this from going out of
  // scope with unfreed pages.
  pft.unregister_allocator(*this);
}

SimplePFA::SimplePFA(PageFrameTable &pft, uint64_t _start, uint64_t _end)
    : PageFrameAllocator(pft, _start, _end) {
  // Mark unusable pages.
  uint64_t unusable_pg_it = start;
  for (auto &region : get_usable_regions()) {
    if (region.base < start) {
      // Skip regions until we've reached the start of our
      // responsible region.
      continue;
    }
    while (unusable_pg_it < region.base && unusable_pg_it < end) {
      pft.get_pfd(unusable_pg_it).unusable = true;
      unusable_pg_it += PG_SZ;
    }
    if (unusable_pg_it >= end) {
      // Stop once we've reached the end of our responsible region.
      break;
    }
    unusable_pg_it += region.len;
  }
}

std::optional<uint64_t> SimplePFA::alloc(unsigned num_pg) {
  uint64_t len = (end - start) / PG_SZ;

  if (num_pg > len) {
    return {};
  }

  // Detect if we've wrapped all the way around.
  unsigned steps = 0;

  auto try_alloc = [&] {
    if (needle + num_pg > endp) {
      // Need to wrap, so not contiguous.
      steps += endp - needle;
      needle = startp;
      return false;
    }
    for (auto *it = needle; it < needle + num_pg; ++it) {
      if (!it->usable()) {
        steps += it - needle;
        needle = it;
        return false;
      }
    }
    return true;
  };

  while (!try_alloc() && likely(steps < len)) {
    // Find next location to try.
    while (!needle->usable() && likely(++steps < len)) {
      ++needle;
      if (unlikely(needle >= endp)) {
        needle = startp;
      }
    }
  }

  if (steps >= len) {
    // We've wrapped around and couldn't find anywhere to allocate.
    return {};
  }

  // Actually do the allocation.
  for (auto *it = needle; it < needle + num_pg; ++it) {
    it->allocated = true;
  }
  DEBUG_ASSERT(alloced_pgs <= get_total_pages());
  alloced_pgs += num_pg;
  return pft.get_pf_offset(*needle) << PG_SZ_BITS;
}

void SimplePFA::free(uint64_t base, uint64_t num_pg) {
  ASSERT(util::algorithm::aligned_pow2<PG_SZ>(base));
  ASSERT(base >= start && base + num_pg <= end);

  for (auto *startp = &pft.get_pfd(base), *it = startp; it < startp + num_pg;
       ++it) {
    ASSERT(it->allocated);
    it->allocated = false;
  }

  DEBUG_ASSERT(alloced_pgs >= num_pg);
  alloced_pgs -= num_pg;
}

} // namespace mem::phys
