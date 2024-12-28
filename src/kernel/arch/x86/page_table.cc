#include "page_table.h"
#include "memdefs.h"
#include "mm/virt.h"
#include "nonstd/libc.h"
#include "perf.h"
#include <cstddef>

namespace {

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

} // namespace

namespace arch::page_table {
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
          reinterpret_cast<const PageTableEntry *>(table[i].addr << PG_SZ_BITS),
          i);
    }
  }
}

bool map(uint64_t phys, void *virt, bool uncacheable) {
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
    // (not supported by HmmOS).
    assert(!pde.ps);
  } else {
    // Allocate and clear a new page table.
    auto *page_table = ::operator new(PG_SZ);
    if (unlikely(page_table == nullptr)) {
      return false;
    }
    nonstd::memset(page_table, 0, PG_SZ);

    // Initialize page directory entry.
    nonstd::memset(&pde, 0, sizeof pde);
    pde.p = 1;
    pde.r_w = 1;
    pde.u_s = 0;
    pde.pwt = 0;
    pde.pcd = 0;
    pde.a = 0;
    pde.ps = 0;
    pde.addr = mem::virt::hhdm_to_direct(page_table) >> PG_SZ_BITS;
  }

  // Index in PT (second 10 bits).
  auto *pt = mem::virt::direct_to_hhdm<PageTableEntry>(pde.addr << PG_SZ_BITS);
  unsigned pt_idx = ((size_t)virt >> PG_SZ_BITS) & 0x03FF;
  auto &pte = pt[pt_idx];

  // We don't support overwriting a page table entry at the
  // moment. This can be added in the future with an appropriate
  // change to the interface. For now we assume as a precondition that
  // if the page is possibly mapped, the caller unmaps the page first.
  assert(!pte.p);

  nonstd::memset(&pte, 0, sizeof pte);
  pte.p = 1;
  pte.r_w = 1;
  pte.u_s = 0;
  pte.pwt = 0;
  pte.pcd = uncacheable;
  pte.a = 0;
  pte.d = 0;
  pte.pat = 0;
  // Assume this is kernel memory and should be mapped globally.
  pte.g = 1;
  pte.addr = phys >> PG_SZ_BITS;

  // I don't think we need to invlpg here since we do it when
  // unmapping pages.
  return true;
}

bool unmap(void *virt) {
  auto *pte = fetch_pte(virt);
  if (pte == nullptr || !pte->p) {
    return false;
  }
  pte->p = 0;
  // TODO: we may not always need the invalpg here, e.g., if we are
  // switching out the entire virtual mapping by writing CR3 when
  // context switching. For now always invalpg for simplicity.
  __asm__ volatile("invlpg %0" : : "m"(*(unsigned *)virt));
  return true;
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

} // namespace arch::page_table
