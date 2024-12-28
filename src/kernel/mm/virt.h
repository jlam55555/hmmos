#pragma once

/// \file virt.h
/// \brief Virtual memory mapping
///
/// The virtual memory space is roughly partitioned like so (with
/// values of KERNEL_MAP_SZ and IO_MAP_SZ at time of writing):
///
/// +-----------------------+------+--------------------------------+
/// | 0xFE000000-0xFFFFFFFF | 32MB | Kernel map; mapped by          |
/// |                       |      | bootloader                     |
/// |---------------------------------------------------------------|
/// | 0xFDC00000-0xFDFFFFFF |  4MB | Device mapped memory/IO ports; |
/// |                       |      | mapped by kernel on startup    |
/// |---------------------------------------------------------------|
/// | 0xC0000000-0xFDBFFFFF | ~1GB | HHDM; mapped by bootloader     |
/// |---------------------------------------------------------------|
/// | 0x00000000-0xBFFFFFFF |  3GB | (userspace) low memory;        |
/// |                       |      | mapped by kernel, swapped      |
/// |                       |      | on context switch              |
/// +-----------------------+------+--------------------------------+
///
/// The memory-map/IO port region and low memory may not be completely
/// mapped. The kernel map and HHDM should be completely mapped.
///
/// Technically the kernel map doesn't have to be independent of the
/// HHDM, but it's what Limine did and I copied it. From a practical
/// perspective, it's useful to distinguish kernel text/data addresses
/// from kernel heap-allocated structures, and we can possibly use
/// different caching for the kernel virtual addresses.
///

#include "boot_protocol.h"
#include "memdefs.h"
#include "page_table.h"
#include <cassert>
#include <cstddef>
#include <functional>

namespace mem::virt {

// See comment at top of file. These macros are shared with bootloader
// code, since the bootloader sets up most of the virtual memory
// mappings. {kernel,io}_map_start should be void* but that's not
// constexpr.
constexpr size_t kernel_map_sz = KERNEL_MAP_SZ;
constexpr size_t kernel_map_start = 4 * GB - kernel_map_sz;

constexpr size_t io_map_sz = IO_MAP_SZ;
constexpr size_t io_map_start = kernel_map_start - io_map_sz;

constexpr size_t hhdm_len = 1 * GB - (kernel_map_sz + io_map_sz);
constexpr size_t hhdm_start = HM_START;

// These memory regions should partition the entire 32-bit virtual
// address space.
static_assert(hhdm_start + hhdm_len + io_map_sz + kernel_map_sz == 4 * GB);

template <typename T = void> inline T *direct_to_hhdm(uint64_t phys_addr) {
  assert(phys_addr < hhdm_len);
  return reinterpret_cast<T *>(phys_addr + hhdm_start);
}

inline uint64_t hhdm_to_direct(void *_virt_addr) {
  size_t virt_addr = reinterpret_cast<size_t>(_virt_addr);
  assert(virt_addr >= hhdm_start && virt_addr < hhdm_start + hhdm_len);
  return virt_addr - hhdm_start;
}

/// Walk page tables and log page mappings.
inline void enumerate_page_tables() {
  arch::page_table::enumerate_page_tables();
}

/// Maps the contiguous virtual memory region of \a pg pages starting
/// at \a virt to the contiguous physical memory region starting at \a
/// phys. Mark these pages as uncacheable. Intended for mapping
/// memory-mapped or IO port physical addresses (usually assigned by
/// firmware) into the virtual address space.
///
/// This will fail if any of the pages in the virtual address range
/// are already mapped.
///
/// \note This is based on a layman's understanding of Linux's
/// ioremap() interface, but should not be held to any of the same
/// requirements or guarantees.
///
/// \param phys Start of contiguous physical memory region to map to
/// \param phys Start of contiguous virtual memory region to map from
/// \param pg Number of pages to map
/// \return true on success
///
bool ioremap(uint64_t phys, void *virt, unsigned pg);

/// Allocate virtual memory pages for use with \ref ioremap(). These
/// pages will be allocated from the virtual memory hole reserved for
/// this purpose (starting at \ref io_map_start).
///
/// This assumes the memory is never freed.
///
/// \param pg The number of contiguous 4KB virtual memory pages to
///           allocate
/// \return The start of the contiguous virtual memory region, or
///         nullptr if allocation failed
void *io_alloc(unsigned pg);

/// Allocates \a pg physical pages, and map the contiguous virtual
/// memory region starting at \a virt to these physical
/// pages. Intended for use in allocating memory for userspace
/// processes.
///
/// This will fail if we OOM when allocating physical pages, or if any
/// of the pages in the virtual address range are already mapped.
///
/// \note The allocated physical pages may not be contiguous.
///
/// \note This is based on a layman's understanding of Linux's
/// vmalloc() interface, but should not be held to any of the same
/// requirements or guarantees.
///
/// \param virt Start of the virtual memory region to map
/// \param pg Number of pages to allocate
/// \return true on success
///
bool vmalloc(void *virt, unsigned pg);

/// Maps the (4KB) virtual memory page \a virt to the physical memory
/// page \a phys.
///
/// \note This is a lower-level interface -- you should usually use
/// \ref ioremap() or \ref vmalloc() to map pages into the virtual
/// memory space.
///
/// \param phys Physical page to map to
/// \param virt Virtual page to map from
/// \param uncacheable Whether page should be marked uncacheable
/// \return true on success
///
inline bool map(uint64_t phys, void *virt, bool uncacheable = false) {
  return arch::page_table::map(phys, virt, uncacheable);
}

/// Unmaps the (4KB) page containing the given virtual address from
/// the page table.
///
/// \return true on success
///
inline bool unmap(void *virt) { return arch::page_table::unmap(virt); }

/// Update virtual -> physical page table mapping attributes.
///
inline bool mark_uncacheable(void *virt) {
  return arch::page_table::mark_uncacheable(virt);
}

} // namespace mem::virt
