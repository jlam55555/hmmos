#include "virt.h"
#include "memdefs.h"
#include "mm/kmalloc.h"
#include "perf.h"

namespace mem::virt {

bool ioremap(uint64_t phys, void *virt, unsigned pg) {
  for (int i = 0; i < pg; ++i) {
    if (unlikely(!map(phys + (i << PG_SZ_BITS),
                      (void *)((uint32_t)virt + (i << PG_SZ_BITS)),
                      /*uncacheable=*/true))) {
      return false;
    }
  }
  return true;
}

void *io_alloc(unsigned pg) {
  static size_t io_region_offset = 0;
  if (io_region_offset + (pg << PG_SZ_BITS) > io_map_sz) {
    return nullptr;
  }
  void *res = (void *)(io_map_start + io_region_offset);
  io_region_offset += (pg << PG_SZ_BITS);
  return res;
}

bool vmalloc(void *virt, unsigned pg) {
  /// TODO: This should attempt to allocate pages past the first 1GB of
  /// memory, and possibly fallback to the first 1GB of memory.
  /// Otherwise it'll eat into kernel memory (memory reachable via
  /// HHDM).
  ///
  for (unsigned i = 0; i < pg; ++i) {
    // We can allocate more pages at a time, but single-page
    // allocation is faster/more likely to succeed.
    //
    // Note: the kernel heap implementation ensures that this is
    // page-aligned.
    if (void *pg = ::operator new(PG_SZ);
        unlikely(pg == nullptr) ||
        unlikely(!map(hhdm_to_direct(pg),
                      (void *)((size_t)virt + (i << PG_SZ_BITS))))) {
      return false;
    }
  }
  return true;
}

} // namespace mem::virt
