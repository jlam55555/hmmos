#include "mm/kmalloc.h"
#include "memdefs.h"
#include "mm/page_frame_allocator.h"
#include "mm/virt.h"
#include "perf.h"
#include "util/algorithm.h"

namespace {
void *arena = nullptr;
mem::phys::SimplePFA *pfa = nullptr;
}; // namespace

namespace mem {

void set_pfa(phys::SimplePFA *_pfa) { pfa = _pfa; }

void *kmalloc(size_t sz) noexcept {
  if (unlikely(pfa == nullptr)) {
    return nullptr;
  }

  // Alloc new page(s) as needed.
  if (!arena || ((size_t)arena & (PG_SZ - 1)) + sz > PG_SZ) {
    auto page_frame = pfa->alloc(util::algorithm::ceil_pow2<PG_SZ>(sz) / PG_SZ);
    arena = page_frame ? virt::direct_to_hhdm(*page_frame) : nullptr;
  }

  if (!arena) {
    // OOM.
    return nullptr;
  }

  void *obj = arena;
  arena = (char *)arena + sz;
  if (PG_ALIGNED((size_t)arena)) {
    // We've reached the end of a page, and will have to alloc a new
    // page on the next kmalloc() call.
    arena = nullptr;
  }
  return obj;
}

void kfree(void *data) noexcept {
  // nop
}

} // namespace mem

void *operator new(size_t sz) { return ::operator new(sz, std::nothrow); }
void *operator new(size_t sz, const std::nothrow_t &tag) noexcept {
  return mem::kmalloc(sz);
}
void operator delete(void *ptr) noexcept { return mem::kfree(ptr); }
