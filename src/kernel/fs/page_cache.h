#pragma once

/// \file page_cache.h
/// \brief LRU cache for disk-backed pages

#include "drivers/device.h"
#include "fs/vfs.h"
#include "mm/page_frame_table.h"
#include "nonstd/hash_bytes.h"
#include "util/lru.h"

namespace fs::cache {

/// Page cache key. Uniquely identifies a page via (device, offset).
struct PageCacheKey {
  drivers::BlockDevice *dev;
  uint64_t offset = 0;

  bool operator==(const PageCacheKey &other) const {
    return nonstd::memcmp(this, &other, sizeof *this) == 0;
  }
};

/// Page cache entry. Wrapper around PageFrameDescriptor that does
/// cleanup on eviction (destruction).
struct PageCacheEntry {
  mem::phys::PageFrameDescriptor *page;

  // Store and remove metadata from the PFT.
  PageCacheEntry(mem::phys::PageFrameDescriptor *_page,
                 drivers::BlockDevice *dev);
  ~PageCacheEntry();
};

/// The page cache is a transparent cache (except for O_DIRECT I/O)
/// that sits between block device drivers and filesystem drivers. The
/// core interface is \a get_page(). There are also interfaces to
/// flush and evict pages.
///
/// **Cache key**: Pages in the cache are keyed by (device,
/// offset). The page cache has no concept of files/inodes, and
/// assumes there is no aliasing between different devices. When
/// fetching a page for an offset within an inode, it's expected that
/// the inode offset is first translated to the device offset before
/// lookup in the page cache. The filesystem layer can use linear
/// block offsets directly.
///
/// **Flushing**: Dirty pages will be flushed to the device dirty via
/// one of the following mechanisms: (1) the process calling \a
/// msync(); (2) when the page is evicted; or (3) by a
/// buffer-dirty-flush (bdflush) worker thread (TODO).
///
/// **Page eviction and rmap**: Page cache pages may be marked as
/// locked. Unlocked pages may be evicted either (1) for a specific
/// page via O_DIRECT I/O; or (2) by evicting pages in LRU manner. (2)
/// may happen automatically under memory pressure, or manually by the
/// user (analogous to /proc/sys/vm/drop_caches in Linux). Dirty
/// evicted pages are first flushed to device. HmmOS doesn't support
/// paging, so non-file backed pages must be locked. Pages must also
/// be unmapped from any process that maintains a memory mapping to
/// that page (see \ref proc::Process::mmap() for more about
/// unmapping). Finding the mappings uses a reverse-mapping technique
/// ("rmap" in Linux) -- the kernel iterates a linked list of all VMAs
/// mapping this file.
///
/// **O_DIRECT I/O**: Read and write operations with the O_DIRECT flag
/// bypass the page cache. However, the page cache still needs to be
/// flushed+invalidated before either reads or writes to ensure
/// coherency within the same process. The kernel does not do any
/// locking to ensure consistency between flushes and other I/O, and
/// it does not ensure that O_DIRECT I/O is page-aligned, so mixing
/// O_DIRECT and non-O_DIRECT I/O on the same file is UB.
///
/// **Memory-mapped pages**: The mmap() syscall uses the file cache for
/// all shared memory mappings. See \ref proc::Process::mmap().
///
/// **Sector vs. page**: All operations are page sized. Reads to a
/// 512B sector will cause the 4KB page to be loaded into the
/// cache. Flushes of a 4KB dirty page will cause all sectors to be
/// written back. (TODO: implement per-sector dirtiness tracking.)
using PageCache = util::LRUCache<PageCacheKey, PageCacheEntry>;

/// Returns a page cache entry containing the PFD that maps the
/// file-backed page. Note that the page may not be mapped into
/// virtual memory (\see VirtLease::get()).
PageCache::Lease get(const Inode &inode, uint64_t offset);

/// Same as above for raw offset within a block (not associated with a
/// file).
PageCache::Lease get(drivers::BlockDevice &device, uint64_t offset);

struct VirtLease {
public:
  VirtLease(auto &owner, uint64_t offset);
  ~VirtLease();
  std::byte *get() const;

private:
  PageCache::Lease page_cache_lease;
};

/// Get a page from the page cache and temporarily map it into
/// memory. This uses a shared global tmp buffer. Returns a RAII
/// object containing the object.

void init();

} // namespace fs::cache

namespace nonstd {
template <> struct hash<fs::cache::PageCacheKey> {
  size_t operator()(const fs::cache::PageCacheKey &key) {
    return hash_bytes(&key, sizeof key);
  }
};
} // namespace nonstd
