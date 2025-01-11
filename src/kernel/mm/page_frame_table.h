#pragma once

/// \file
/// \brief Page frame descriptors and page frame table
///
/// There will be a page frame descriptor (PFD) for each physical 4KB
/// page, for the entirety of the physical linear address space. This
/// includes any gaps in usable memory (so that mapping linear
/// addresses to page frames is simple), although these pages will be
/// marked as unusable. The array of PFDs is called the page frame
/// table (PFT), and it is constructed using the E820 memory map.
///
/// Allocation of page frames is done by a page frame allocator (PFA),
/// which is responsible for some (sub)range of the physical memory
/// space.

#include "boot_protocol.h"
#include "libc_minimal.h"
#include "util/assert.h"
#include "util/intrusive_list.h"
#include "util/objutil.h"

#include <optional>
#include <span>

namespace mem::phys {

/// \brief Metadata for a single page frame in the linear address range.
///
/// For now, make this 64B, same as in the Linux kernel.
class PageFrameDescriptor {
public:
  bool allocated : 1 = false;

  /// "Unusable" rather than "usable" since this is faster to
  /// initialize. This is used by SimplePFA as it doesn't allocate any
  /// auxiliary memory to determine which pages are usable.
  bool unusable : 1 = false;

  bool rsv1 : 6 = false;

  char data[63];

  bool usable() const { return !allocated && !unusable; }
} __attribute__((packed, aligned(64)));

static_assert(sizeof(PageFrameDescriptor) == 64);

class PageFrameAllocator;

/// \brief An array containing one page frame descriptor for each
/// physical 4KB page.
///
/// This is independent of the physical memory allocator.
class PageFrameTable {
public:
  /// Normalize the E820 memory map. We're going to make the
  /// simplifying assumption that no regions (except bootloader
  /// regions) overlap other regions, but otherwise there are no
  /// assumptions.
  ///
  /// The bootloader simply adds its own memory usage to the memory
  /// map (currently this is the bootloader stack, bootloader text,
  /// and page tables). These should overlap with (and be completely
  /// contained) inside a usable memory region.
  ///
  /// The output memory map will be sorted and only contain usable
  /// memory regions. This is guaranteed to have no more entries than
  /// the input E820 memory map (including bootloader entries). (This
  /// is an implementation detail -- it means that we don't need to
  /// allocate any memory, but rather modify the E820 map in-place.)
  PageFrameTable(std::span<e820_mm_entry> mm);

  NON_MOVABLE(PageFrameTable);

  /// \brief Get PFD from offset.
  ///
  PageFrameDescriptor &get_pfd(uint64_t pf_offset) {
    size_t pfd_idx = pf_offset >> PG_SZ_BITS;
    // Technically this should be a strict comparison, but pfd_idx ==
    // pft.size() is allowed to get an end pointer.
    DEBUG_ASSERT(pfd_idx <= pft.size());
    return pft[pfd_idx];
  }

  /// \brief Get offset from PFD.
  ///
  uint64_t get_pf_offset(PageFrameDescriptor &pfd) {
    DEBUG_ASSERT(&pfd >= pft.data() && &pfd < pft.data() + pft.size());
    return &pfd - pft.data();
  }

  /// \brief Return the end of physical memory.
  ///
  /// Used to initialize allocators that are responsible for the
  /// entire physical memory space.
  uint64_t mem_limit() const {
    return usable_regions.back().base + usable_regions.back().len;
  }

  /// \brief Total number of bytes in the memory map (i.e., not
  /// including memory holes nor bootloader memory).
  const uint64_t total_mem_bytes;

protected:
  /// Constructor for test usage.
  PageFrameTable(std::span<e820_mm_entry> mm,
                 std::optional<std::span<PageFrameDescriptor>> pft);

  std::span<const e820_mm_entry> get_usable_regions() { return usable_regions; }

private:
  // Helper functions to construct const members.
  uint64_t compute_total_mem(const std::span<e820_mm_entry> mm) const;
  uint64_t compute_usable_mem() const;
  std::span<e820_mm_entry> normalize_mm(std::span<e820_mm_entry> mm) const;
  std::span<PageFrameDescriptor> alloc_pft() const;

  /// These are used to manage all allocators for this
  /// PFT. Corresponding PFD entries are zeroed upon registering an
  /// allocator. (They're not cleaned up on destruction, but this can
  /// be done in the future if needed.)
  ///
  /// TODO: we should introduce some sort of access-key like mechanism
  /// to avoid friending the class. But this is okay for now.
  friend class PageFrameAllocator;
  bool register_allocator(PageFrameAllocator &allocator, uint64_t start,
                          uint64_t end);
  void unregister_allocator(PageFrameAllocator &allocator);
  util::IntrusiveListHead<PageFrameAllocator> allocators;

  /// \brief Copy of the input E820 memory map.
  ///
  /// We need a copy since the original may live in
  /// bootloader-reclaimable memory, which is cleared.
  std::array<e820_mm_entry, 32> mm_copy;

  /// Dynamically-sized view of the memory map. Points into \ref
  /// mm_copy. Just for convenience.
  const std::span<e820_mm_entry> mm;

  /// \brief List of usable memory regions.
  ///
  /// These are guaranteed to be 4KB-aligned, non-overlapping, and
  /// sorted. To be used by the allocator.
  const std::span<e820_mm_entry> usable_regions;

  /// \brief Backing store for the PFT.
  ///
  const std::span<PageFrameDescriptor> pft;

public:
  /// \brief Total number of usable bytes.
  ///
  // This is at the end because it needs to be
  // initialized after the page frame table is allocated.
  const uint64_t usable_mem_bytes;
};

} // namespace mem::phys
