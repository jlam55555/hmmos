#include "fs/page_cache.h"
#include "mm/kmalloc.h"
#include "mm/page_frame_allocator.h"
#include "mm/virt.h"

#include <functional>

namespace fs::cache {

namespace {

static PageCache *cache = nullptr;

/// A virtual address to use when populating pages into the page
/// cache. This is simply a mapp
std::byte *buf = nullptr;

/// Allocates a physical memory page and populates the page from disk.
PageCacheEntry fetch_page(const PageCacheKey &key) {
  // TODO: appropriate locking around this segment
  // TODO: don't allocate from HHDM, these pages don't need to be mapped
  void *pg = ::operator new(PG_SZ);
  assert(pg != nullptr);
  uint64_t paddr = mem::virt::hhdm_to_direct(pg);
  assert(key.dev->read_page_direct(paddr, key.offset));
  mem::phys::PageFrameDescriptor *pfd = &mem::get_pfa()->get_pfd(paddr);
  return PageCacheEntry{pfd, key.dev};
}

} // namespace

// NOCOMMIT
PageCacheEntry::PageCacheEntry(mem::phys::PageFrameDescriptor *_page,
                               drivers::BlockDevice *dev)
    : page{_page} {
  // TODO: store _dev into the page frame descriptor so that we know
  // how to flush the page and clear it on destruction
}
PageCacheEntry::~PageCacheEntry() = default;

void init() {
  cache = new PageCache{fetch_page, [](auto &&...) {}};

  buf = reinterpret_cast<std::byte *>(mem::virt::io_alloc(1));
  assert(buf != nullptr);
}

PageCache::Lease get(const Inode &inode, uint64_t offset) {
  return get(inode.fs.dev, inode.get_dev_offset(offset));
}

PageCache::Lease get(drivers::BlockDevice &dev, uint64_t offset) {
  return cache->get(PageCacheKey{.dev = &dev, .offset = offset});
}

VirtLease::VirtLease(auto &owner, uint64_t offset)
    : page_cache_lease{fs::cache::get(owner, offset)} {
  const uint64_t paddr =
      mem::get_pfa()->get_paddr(*page_cache_lease.value().page);
  assert(mem::virt::map(paddr, buf, /*userspace=*/false, /*writable=*/true));
}
template VirtLease::VirtLease(Inode &, uint64_t);
template VirtLease::VirtLease(drivers::BlockDevice &, uint64_t);

VirtLease::~VirtLease() { mem::virt::unmap(buf); }
std::byte *VirtLease::get() const { return buf; }

} // namespace fs::cache
