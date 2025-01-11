#include "page_frame_table.h"

#include "boot_protocol.h"
#include "libc_minimal.h"
#include "mm/page_frame_allocator.h"
#include "mm/virt.h"
#include "util/algorithm.h"

#include <algorithm>

namespace mem::phys {

PageFrameTable::PageFrameTable(std::span<e820_mm_entry> mm)
    : PageFrameTable(mm, std::nullopt) {}

PageFrameTable::PageFrameTable(
    std::span<e820_mm_entry> _mm,
    std::optional<std::span<PageFrameDescriptor>> _pft)
    : total_mem_bytes{compute_total_mem(_mm)}, mm_copy{[_mm, this] {
        ASSERT(_mm.size() <= mm_copy.size());
        decltype(mm_copy) rv{};
        std::copy(_mm.begin(), _mm.end(), rv.begin());
        return rv;
      }()},
      mm{mm_copy.begin(), _mm.size()}, usable_regions{normalize_mm(mm)},
      pft{_pft ? *_pft : alloc_pft()}, usable_mem_bytes{compute_usable_mem()} {

  // Also zero bootloader-reclaimable memory. We observe slowdowns
  // when trying to write to the same pages as executable instructions
  // in QEMU so this helps with performance. (This is also a good
  // thing for general security.)
  //
  // Note that any data structures within the bootloader reclaimable
  // region will be zeroed after this (e.g., the E820 memory map), so
  // we need to use/copy them before this point.
  for (const auto mm_entry : mm) {
    if (mm_entry.type == E820_MM_TYPE_BOOTLOADER_RECLAIMABLE) {
      nonstd::memset(virt::direct_to_hhdm(mm_entry.base), 0, mm_entry.len);
    }
  }

  // If PFT was provided, check that it is valid (large enough). If
  // it is larger than implied by the memory map, we won't use the
  // extra slots.
  if (_pft) {
    uint64_t mem_limit = usable_regions.back().base + usable_regions.back().len;
    size_t page_limit = mem_limit >> PG_SZ_BITS;
    ASSERT(_pft->size() >= page_limit);
  }
}

uint64_t PageFrameTable::compute_total_mem(std::span<e820_mm_entry> mm) const {
  uint64_t rval = 0;
  for (const auto &entry : mm) {
    if (entry.type != E820_MM_TYPE_BOOTLOADER) {
      rval += entry.len;
    }
  }
  return rval;
}

uint64_t PageFrameTable::compute_usable_mem() const {
  uint64_t rval = 0;
  for (const auto &entry : usable_regions) {
    rval += entry.len;
  }
  return rval;
}

std::span<e820_mm_entry>
PageFrameTable::normalize_mm(std::span<e820_mm_entry> mm) const {
  // Bookkeeping: total number of bytes in the memory map (i.e., not
  // including memory holes nor bootloader memory).
  uint64_t total_mem_bytes = 0;

  // First, "split" usable memory regions that contain bootloader
  // memory regions since they are unusable.
  for (unsigned i = 0; i < mm.size(); ++i) {
    if (mm[i].type != E820_MM_TYPE_BOOTLOADER) {
      total_mem_bytes += mm[i].len;
      continue;
    }

    auto &bootloader_entry = mm[i];
    for (unsigned j = 0; j < mm.size(); ++j) {
      if (i == j) {
        continue;
      }
      auto &non_bootloader_entry = mm[j];

      if (!util::algorithm::range_overlaps2(bootloader_entry.base,     //
                                            bootloader_entry.len,      //
                                            non_bootloader_entry.base, //
                                            non_bootloader_entry.len,  //
                                            /*inclusive=*/false)) {
        continue;
      }
      ASSERT(non_bootloader_entry.type == E820_MM_TYPE_USABLE);
      ASSERT(util::algorithm::range_subsumes2(non_bootloader_entry.base, //
                                              non_bootloader_entry.len,  //
                                              bootloader_entry.base,     //
                                              bootloader_entry.len));

      // Split the usable region into two usable regions. The
      // bootloader region will be repurposed as a usable region.
      uint64_t base1 = non_bootloader_entry.base;
      uint64_t len1 = bootloader_entry.base - non_bootloader_entry.base;
      uint64_t base2 = bootloader_entry.base + bootloader_entry.len;
      uint64_t len2 =
          non_bootloader_entry.base + non_bootloader_entry.len - base2;
      ASSERT(len1 + len2 + bootloader_entry.len == non_bootloader_entry.len);

      non_bootloader_entry.base = base1;
      non_bootloader_entry.len = len1;
      bootloader_entry.base = base2;
      bootloader_entry.len = len2;
      bootloader_entry.type = E820_MM_TYPE_USABLE;
      break;
    }
  }

  std::sort(mm.begin(), mm.end(),
            [](auto &e1, auto &e2) { return e1.base < e2.base; });

  // Check that no regions are overlapping.
  for (unsigned i = 1; i < mm.size(); ++i) {
    // Bootloader-reclaimable regions will overlap with usable
    // regions, but we want to discard them. So we can effectively
    // ignore them.
    ASSERT(!util::algorithm::range_overlaps2(mm[i - 1].base, //
                                             mm[i - 1].len,  //
                                             mm[i].base,     //
                                             mm[i].len,      //
                                             /*inclusive=*/false) ||
           (mm[i - 1].type == E820_MM_TYPE_BOOTLOADER_RECLAIMABLE &&
            mm[i].type == E820_MM_TYPE_USABLE) ||
           (mm[i - 1].type == E820_MM_TYPE_USABLE) &&
               mm[i].type == E820_MM_TYPE_BOOTLOADER_RECLAIMABLE);
  }

  // Move usable regions to the front.
  unsigned usable_region_count = 0;
  for (unsigned i = 0; i < mm.size(); ++i) {
    if (mm[i].type == E820_MM_TYPE_USABLE) {
      // Using `std::swap()` rather than `std::move()` since
      // self-move assignment isn't well-defined.
      std::swap(mm[i], mm[usable_region_count++]);
    }
  }
  auto usable_region_map = mm.first(usable_region_count);

  // Align entries to full pages.
  for (auto &region : usable_region_map) {
    uint64_t end = util::algorithm::floor_pow2<PG_SZ>(region.base + region.len);
    region.base = util::algorithm::ceil_pow2<PG_SZ>(region.base);
    region.len = end > region.base ? end - region.base : 0;
  }

  // Remove size-zero entries. (Note that we may still end up with a
  // size-zero entry after alloc_pft()).
  unsigned usable_region_nonempty_count = 0;
  for (unsigned i = 0; i < usable_region_map.size(); ++i) {
    if (usable_region_map[i].len) {
      std::swap(usable_region_map[i],
                usable_region_map[usable_region_nonempty_count++]);
    }
  }

  return usable_region_map.first(usable_region_nonempty_count);
}

std::span<PageFrameDescriptor> PageFrameTable::alloc_pft() const {
  // Allocate the page frame array. We're going to do this by trimming
  // it off the first usable memory region that fits it.
  //
  // Note: We need to be careful to use uint64_t for physical page
  // frame offsets, since x86 _can_ address more than 4GB of
  // _physical_ memory with PAE.
  uint64_t mem_limit = usable_regions.back().base + usable_regions.back().len;
  ASSERT(util::algorithm::aligned_pow2<PG_SZ>(mem_limit));
  size_t page_limit = mem_limit >> PG_SZ_BITS;
  size_t pft_len = page_limit * sizeof(PageFrameDescriptor);

  size_t pft_len_padded = util::algorithm::ceil_pow2<PG_SZ>(pft_len);
  PageFrameDescriptor *pft_base = nullptr;
  for (auto &region : usable_regions) {
    if (region.len >= pft_len_padded) {
      pft_base = virt::direct_to_hhdm<PageFrameDescriptor>(region.base);
      region.base += pft_len_padded;
      region.len -= pft_len_padded;

      // Check that PFT fits in HHDM.
      ASSERT(region.base <= virt::hhdm_len);
      break;
    }
  }
  // Couldn't find a region large enough to hold page frame table.
  ASSERT(pft_base != nullptr);

  return std::span<PageFrameDescriptor>{pft_base, page_limit};
}

bool PageFrameTable::register_allocator(PageFrameAllocator &allocator,
                                        uint64_t start, uint64_t end) {
  ASSERT(util::algorithm::aligned_pow2<PG_SZ>(start));
  ASSERT(util::algorithm::aligned_pow2<PG_SZ>(end));
  ASSERT(allocator.empty());

  for (const auto &other : allocators) {
    if (util::algorithm::range_overlaps(start, end, other.start, other.end,
                                        /*inclusive=*/false)) {
      return false;
    }
  }

  allocators.push_back(allocator);

  // Zero-initialize the corresponding PFT entries.
  auto *start_pfd = &get_pfd(start);
  auto *end_pfd = &get_pfd(end);
  nonstd::memset(start_pfd, 0, (char *)end_pfd - (char *)start_pfd);
  return true;
}

void PageFrameTable::unregister_allocator(PageFrameAllocator &allocator) {
  // We assume that the allocator is in this allocator list. This
  // should only be called by the PFA destructor.
  allocator.erase();
}

} // namespace mem::phys
