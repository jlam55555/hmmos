#include "page_table.h"
#include "memdefs.h"
#include "mm/kmalloc.h"
#include "mm/virt.h"
#include "nonstd/libc.h"
#include "perf.h"
#include "util/algorithm.h"
#include "util/assert.h"
#include <cstddef>

namespace arch::page_table {

/// 4MB page directory table regular (non-hupepage) entry.
struct PageDirectoryEntry {
  bool p : 1;
  bool r_w : 1;
  bool u_s : 1;
  bool pwt : 1;
  bool pcd : 1;
  bool a : 1;
  uint8_t ign0 : 1; // Ignored.
  bool ps : 1;      // If set, use the 4MB version.
  uint8_t ign1 : 4; // Ignored.
  uint32_t addr : 20;
} __attribute__((packed));
static_assert(sizeof(PageDirectoryEntry) == 4, "wrong PageDirectoryEntry size");

/// 4MB page directory table hugepage entry.
///
/// \note The only hugepage usage in HmmOS is set up by the bootloader
/// for the HHDM. For simplicity (for now), HmmOS doesn't attempt to
/// map/unmap any hugepages, and only dealing with 4KB pages. Any
/// attempt to modify the existing HHDM mapping/allocate new hugepages
/// is UB. This struct only exists to properly enumerate the
/// bootloader's HHDM. If we ever want to allow user-space hugepages,
/// we should look into libhugetblfs.
///
struct PageDirectoryHugepageEntry {
  bool p : 1;
  bool r_w : 1;
  bool u_s : 1;
  bool pwt : 1;
  bool pcd : 1;
  bool a : 1;
  bool d : 1;
  bool ps : 1;
  bool g : 1;
  uint8_t ign0 : 3;
  bool pat : 1;
  // High bits of page address (e.g., for PSE-36). We're not going to
  // support that here, so this should always be zero.
  uint16_t addr_ext : 8;
  uint8_t rsv0 : 1;
  uint32_t addr : 10;
} __attribute__((packed));
static_assert(sizeof(PageDirectoryHugepageEntry) == 4,
              "wrong PageDirectoryHugepageEntry size");

/// (4KB) page table entry.
struct PageTableEntry {
  bool p : 1;
  bool r_w : 1;
  bool u_s : 1;
  bool pwt : 1;
  bool pcd : 1;
  bool a : 1;
  bool d : 1;
  bool pat : 1;
  bool g : 1;
  uint8_t ign0 : 3;
  uint32_t addr : 20;
} __attribute__((packed));
static_assert(sizeof(PageTableEntry) == 4, "wrong PageTableEntry size");

// For regular pages, the virtual address mapping comes from:
// - 10 bits PDE index, 10 bits PTE index, 12 bits page offset
//
// For hugepages, the virtual address mapping comes from:
// - 10 bits PDE index, 20 bits page offset
constexpr unsigned directory_table_entries =
    PG_SZ / sizeof(PageDirectoryEntry); // == 1024
constexpr unsigned directory_entry_bits = 10;
constexpr unsigned page_table_entries =
    PG_SZ / sizeof(PageDirectoryEntry); // == 1024
constexpr unsigned page_table_bits = 10;

static_assert(1 << page_table_bits == page_table_entries);
static_assert(1 << directory_entry_bits == directory_table_entries);

PageDirectoryEntry *get_page_directory() {
  size_t table_phys;
  __asm__("movl %%cr3, %0" : "=r"(table_phys));
  return mem::virt::direct_to_hhdm<PageDirectoryEntry>(table_phys);
}

/// Helper function which performs the page table walk.
///
/// \return nullptr if the PDE is not present. The PTE entry will be
/// returned as long as the PDE is present, even if the PTE is itself
/// not present.
///
PageTableEntry *fetch_pte(void *virt) {
  assert(PG_ALIGNED((size_t)virt));

  // Index in PD (high 10 bits).
  auto *pd = get_page_directory();
  unsigned pd_idx = ((size_t)virt >> PG_SZ_BITS) >> directory_entry_bits;
  auto &pde = pd[pd_idx];
  if (!pde.p) {
    return nullptr;
  }

  // 4MB hugepages not supported by HmmOS.
  assert(!pde.ps);

  // Index in PT (second 10 bits).
  auto *pt = mem::virt::direct_to_hhdm<PageTableEntry>(pde.addr << PG_SZ_BITS);
  unsigned pt_idx = ((size_t)virt >> PG_SZ_BITS) & 0x03FF;
  return &pt[pt_idx];
}

void enumerate_page_table(const PageTableEntry *table,
                          unsigned page_table_idx) {
  uint32_t virt_addr_base = page_table_idx * HUGE_PG_SZ;
  for (unsigned i = 0; i < page_table_entries; ++i) {
    if (!table[i].p) {
      continue;
    }
    nonstd::printf("0x%x -> 0x%llx\r\n", virt_addr_base + (i << PG_SZ_BITS),
                   (uint64_t)table[i].addr << PG_SZ_BITS);
  }
}

void enumerate_page_tables() {
  const auto *table = get_page_directory();

  nonstd::printf("virt -> phys\r\n");
  for (unsigned i = 0; i < directory_table_entries; ++i) {
    if (!table[i].p) {
      continue;
    }

    if (table[i].ps) {
      auto &hugepg =
          reinterpret_cast<const PageDirectoryHugepageEntry &>(table[i]);
      nonstd::printf("0x%x -> 0x%llx (huge)\r\n", i * HUGE_PG_SZ,
                     (uint64_t)hugepg.addr * HUGE_PG_SZ);
    } else {
      enumerate_page_table(
          reinterpret_cast<const PageTableEntry *>(
              mem::virt::direct_to_hhdm(table[i].addr << PG_SZ_BITS)),
          i);
    }
  }
}

namespace {

// Allocate and clear a new page table to assign to the page directory
// entry.
PageTableEntry *create_pt(PageDirectoryEntry *pde) {
  if (unlikely(pde->p)) {
    return nullptr;
  }

  auto *page_table = ::operator new(PG_SZ);
  if (unlikely(page_table == nullptr)) {
    return nullptr;
  }
  nonstd::memset(page_table, 0, PG_SZ);

  // Initialize page directory entry.
  nonstd::memset(pde, 0, sizeof *pde);
  pde->p = 1;

  // Always mark PD entries as userspace-accessible and
  // writable. The permissions will be controlled on the page table
  // level.
  pde->r_w = 1;
  pde->u_s = 1;

  pde->pwt = 0;
  pde->pcd = 0;
  pde->a = 0;
  pde->ps = 0;
  pde->addr = mem::virt::hhdm_to_direct(page_table) >> PG_SZ_BITS;

  return reinterpret_cast<PageTableEntry *>(page_table);
}
} // namespace

bool map(uint64_t phys, void *virt, bool u_s, bool r_w, bool uncacheable) {
  assert(PG_ALIGNED(phys));
  assert(PG_ALIGNED((size_t)virt));

  // We have to recreate the logic in \ref fetch_pte() since the PDE
  // may not exist.

  // Index in PD (high 10 bits).
  auto *pd = get_page_directory();
  unsigned pd_idx = ((size_t)virt >> PG_SZ_BITS) >> directory_entry_bits;
  auto &pde = pd[pd_idx];
  if (pde.p) {
    // If page directory entry exists, check that it isn't a hugepage
    // (not supported by HmmOS for userspace mappings).
    assert(!pde.ps);
  } else {
    ASSERT(create_pt(&pde) != nullptr);
  }

  // Index in PT (second 10 bits).
  auto *pt = mem::virt::direct_to_hhdm<PageTableEntry>(pde.addr << PG_SZ_BITS);
  unsigned pt_idx = ((size_t)virt >> PG_SZ_BITS) & 0x03FF;
  auto &pte = pt[pt_idx];

  if (pte.p) {
    // We don't support overwriting a page table entry at the
    // moment. This can be added in the future with an appropriate
    // change to the interface. For now we assume as a precondition
    // that if the page is possibly mapped, the caller unmaps the page
    // first.
    return false;
  }

  nonstd::memset(&pte, 0, sizeof pte);
  pte.p = 1;
  pte.r_w = r_w;
  pte.u_s = u_s;
  pte.pwt = 0;
  pte.pcd = uncacheable;
  pte.a = 0;
  pte.d = 0;
  pte.pat = 0;
  // Assume this is kernel memory and should be mapped globally.
  pte.g = 1;
  pte.addr = phys >> PG_SZ_BITS;

  // I don't think we need to invlpg here since we do it when
  // unmapping pages. It would only be needed if we remap a virtual
  // address, which is explicitly disallowed above.
  return true;
}

bool unmap(void *virt) {
  auto *pte = fetch_pte(virt);
  if (pte == nullptr || !pte->p) {
    return false;
  }
  unmap_at(pte, virt);
  return true;
}

void unmap_at(PageTableEntry *pte, void *virt) {
  DEBUG_ASSERT(pte != nullptr);
  // memset rather than `pte->pt = 0` in case any comparison wants to
  // check page table validity by comparing all bits. This is just a
  // bit safer in case I do something stupid in the future.
  nonstd::memset(pte, 0, sizeof *pte);
  // TODO: we may not always need the invlpg here, e.g., if we are
  // switching out the entire virtual mapping by writing CR3 when
  // context switching. For now always invlpg for simplicity.
  __asm__ volatile("invlpg %0" : : "m"(*(unsigned *)virt));

  nonstd::printf("NOCOMMIT unmapping page at 0x%x\r\n", (unsigned)virt);
}

bool mark_uncacheable(void *virt) {
  auto *pte = fetch_pte(virt);
  if (pte == nullptr || !pte->p) {
    return false;
  }
  pte->pcd = 1;
  __asm__ volatile("invlpg %0" : : "m"(*(unsigned *)virt));
  return true;
}

PageDirectoryEntry *clone_kernel_page_directory(PageDirectoryEntry *orig) {
  // TODO: more complex logic for handilng exec/fork. For exec, we can
  // consider starting from the canonical page directory, but if the
  // code is correct it should be equivalent to copying the current
  // page directory.

  const PageDirectoryEntry *pd = get_page_directory();
  auto *new_pd = reinterpret_cast<PageDirectoryEntry *>(::operator new(PG_SZ));

  // Copy the low memory (userspace) page tables.
  constexpr size_t high_mem_offset =
      mem::virt::hhdm_start / HUGE_PG_SZ * sizeof(PageDirectoryEntry);
  nonstd::memset(new_pd, 0, high_mem_offset);
  static_assert(high_mem_offset == 0x0C00);
  static_assert(
      util::algorithm::aligned_pow2<HUGE_PG_SZ>(mem::virt::hhdm_start));
  for (unsigned i = 0; i < mem::virt::hhdm_start / HUGE_PG_SZ; ++i) {
    if (!pd[i].p) {
      continue;
    }

    if (pd[i].ps) {
      new_pd[i] = pd[i];
    } else {
      // TODO: don't copy the page if all zeros/non present
      PageTableEntry *page_table =
          reinterpret_cast<PageTableEntry *>(::operator new(PG_SZ));
      nonstd::memcpy(page_table,
                     mem::virt::direct_to_hhdm(pd[i].addr << PG_SZ_BITS),
                     PG_SZ);
      new_pd[i] = pd[i];
      new_pd[i].addr = mem::virt::hhdm_to_direct(page_table) >> PG_SZ_BITS;
    }
  }

  // Copy the high memory (kernel) page tables.
  nonstd::memcpy((std::byte *)new_pd + high_mem_offset,
                 (std::byte *)pd + high_mem_offset, PG_SZ - high_mem_offset);
  return new_pd;
}

void delete_cloned_page_directory(PageDirectoryEntry *pd) {
  constexpr size_t high_mem_offset =
      mem::virt::hhdm_start / HUGE_PG_SZ * sizeof(PageDirectoryEntry);
  static_assert(high_mem_offset == 0x0C00);
  static_assert(
      util::algorithm::aligned_pow2<HUGE_PG_SZ>(mem::virt::hhdm_start));
  for (unsigned i = 0; i < mem::virt::hhdm_start / HUGE_PG_SZ; ++i) {
    if (!pd[i].p || pd[i].ps) {
      // nothing to do
      continue;
    }

    auto *pt = mem::virt::direct_to_hhdm(pd[i].addr << PG_SZ_BITS);
    // TODO: assert that the whole page is empty
    ::operator delete(pt);
  }
  ::operator delete(pd);

  // To be safe, let's revert back to the kernel canonical mapping.
  set_page_directory(get_canonical_pt());
}

void set_page_directory(PageDirectoryEntry *pde) {
  size_t table_phys = mem::virt::hhdm_to_direct(pde);
  __asm__("movl %0, %%cr3" ::"r"(table_phys));
}

void iter_page_table(int32_t start_pg, int32_t end_pg,
                     void (*cb)(PageTableEntry *pte, void *vaddr)) {
  const auto *pd = get_page_directory();
  for (unsigned i = start_pg / page_table_entries;
       i <= end_pg / page_table_entries; ++i) {
    if (!pd[i].p || pd[i].ps) {
      // We never map userspace using hugepages.
      continue;
    }
    auto pt = reinterpret_cast<PageTableEntry *>(
        mem::virt::direct_to_hhdm(pd[i].addr << PG_SZ_BITS));
    const auto virt_addr_base = i * HUGE_PG_SZ;
    for (unsigned j = std::max(0U, start_pg - i * page_table_entries);
         j < std::min(page_table_entries, end_pg - i * page_table_entries);
         ++j) {
      cb(&pt[j], (void *)(virt_addr_base + (j << PG_SZ_BITS)));
    }
  }
}

namespace {
PageDirectoryEntry *canonical_pd = nullptr;
}

void setup_canonical_pt() {
  PageDirectoryEntry *pd = get_page_directory();

  // Unmap the lower 1MB direct map. We also want to unlink the page
  // table so we don't iterate it when copying. It's allocated via the
  // bootloader so we can't actually free the page normally, so let's
  // mark the PageDirectory entry as unused. Nothing else in the
  // kernel should be using low memory so this is safe.
  iter_page_table(0, 1 * MB / PG_SZ, unmap_at);
  // See note in \ref unmap_at() about memset rather than unsetting
  // the presence bit.
  nonstd::memset(&pd[0], 0, sizeof pd[0]);

  // Create a page table for the I/O memory hole page directory entry.
  // So that this is part of the page table that never needs to be
  // copied.
  static_assert(IO_MAP_SZ == HUGE_PG_SZ);
  create_pt(&pd[mem::virt::io_map_start / HUGE_PG_SZ]);

  // Set this as the canonical kernel page directory.
  canonical_pd = pd;
}

PageDirectoryEntry *get_canonical_pt() {
  ASSERT(canonical_pd != nullptr);
  return canonical_pd;
}

} // namespace arch::page_table
