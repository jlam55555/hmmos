#include "page_table.h"
#include "boot_protocol.h"
#include "mbr.h"
#include "memdefs.h"
#include "pmode_print.h"
#include "string.h"
#include <assert.h>
#include <stddef.h>

/// Simplified printing function. Ignore ACPI attributes for now.
static void _e820_entry_print(struct e820_mm_entry *const ent) {
  pmode_puts("  base=");
  pmode_printq(ent->base);
  pmode_puts(" len=");
  pmode_printq(ent->len);
  pmode_puts(" type=");
  pmode_printb(ent->type);
  pmode_puts("\r\n");
}

/// Parse and print the memory map. We could do this in real mode when
/// querying the BIOS, but it's easier to do it now (and we only need
/// it now for setting up the page table).
void e820_mm_print() {
  struct e820_mm_entry *ent;
  pmode_puts("E820 memory map:\r\n");
  for (ent = e820_mem_map; e820_entry_present(ent); ++ent) {
    _e820_entry_print(ent);
  }
  pmode_puts("Number of entries: ");
  pmode_printb(ent - e820_mem_map);
  pmode_puts("\r\n");
}

void *e820_alloc(unsigned len, bool hugepg_align) {
  // Really stupid allocation method for now since this only gets used
  // twice: loop one (huge)page at a time in the first GB until we find a
  // free area of this size.
  unsigned incr = hugepg_align ? HUGE_PG_SZ : PG_SZ;
  for (uint64_t base = incr; base < (1 * GB); base += incr) {
    // Check that this region completely exists in a usable memory
    // region and doesn't overlap any bad memory regions. We need to
    // check both because the memory regions aren't normalized and may
    // overlap.
    bool is_usable = false;
    bool overlaps_unusable = false;
    struct e820_mm_entry *ent;
    for (ent = e820_mem_map; e820_entry_present(ent); ++ent) {
      if (ent->type == E820_MM_TYPE_USABLE) {
        if (base >= ent->base && base + len <= ent->base + ent->len) {
          // Subsumed by a usable memory region.
          is_usable = true;
        }
      } else {
        if (base + len >= ent->base && base <= ent->base + ent->len) {
          // Overlaps an unusable memory region.
          overlaps_unusable = true;
          break;
        }
      }
    }

    if (is_usable && !overlaps_unusable) {
      return (void *)base;
    }
  }
  return NULL;
}

bool e820_augment_bootloader(uint64_t base, uint64_t len) {
  struct e820_mm_entry *ent;
  for (ent = e820_mem_map; e820_entry_present(ent); ++ent) {
  }

  if (ent - e820_mem_map + 1 >= e820_mm_max_entries) {
    // Last entry must be a null entry.
    pmode_puts("failed to add bootloader entry to mm: too many entries\r\n");
    return false;
  }

  // Entry should be zeroed beforehand.
  ent->base = base;
  ent->len = len;
  ent->type = E820_MM_TYPE_BOOTLOADER;
  return true;
}

static void _pt_zero_pg(void *pg) { memset(pg, 0, PG_SZ); }

/// The `_pt_map_(huge)pg()` assume a standard 2-level (non-PAE)
/// paging scheme.
///
/// - Precondition: PSE is enabled before mapping hugepages.
static void _pt_map_pg(struct page_directory_entry *pd,
                       struct page_table_entry *pt, uint32_t va, uint32_t pa,
                       bool map_global) {
  assert(PG_ALIGNED(va));
  assert(PG_ALIGNED(pa));

  const unsigned pd_idx = va >> 22;
  const unsigned pt_idx = (va >> PG_SZ_BITS) & 0x3FF;

  // Set up page directory if not already set.
  pd += pd_idx;

  if (!pd->p) {
    pd->p = 1;
    pd->r_w = 1;
    pd->u_s = 0;
    pd->pwt = 0;
    pd->pcd = 0;
    pd->a = 0;
    pd->ps = 0;
    pd->addr = (uint32_t)pt >> PG_SZ_BITS;
  } else {
    assert(pd->addr == (uint32_t)pt >> PG_SZ_BITS);
  }

  // Set up page table entry.
  pt += pt_idx;
  assert(!pt->p);
  pt->p = 1;
  pt->r_w = 1;
  pt->u_s = 0;
  pt->pwt = 0;
  pt->pcd = 0;
  pt->a = 0;
  pt->d = 0;
  pt->pat = 0;
  pt->g = map_global;
  pt->addr = pa >> PG_SZ_BITS;
}
static void _pt_map_hugepg(struct page_directory_entry_4mb *pd, uint32_t va,
                           uint32_t pa, bool map_global) {
  assert(HUGEPG_ALIGNED(va));
  assert(HUGEPG_ALIGNED(pa));

  const unsigned pd_idx = va >> 22;

  pd += pd_idx;
  assert(!pd->p);
  pd->p = 1;
  pd->r_w = 1;
  pd->u_s = 0;
  pd->pwt = 0;
  pd->pcd = 0;
  pd->a = 0;
  pd->d = 0;
  pd->ps = 1;
  pd->g = map_global;
  pd->pat = 0;
  pd->addr_ext = 0;
  pd->rsv0 = 0;
  pd->addr = pa >> 22;
}

bool pt_setup(void *kernel_paddr) {
  const unsigned dynamic_alloc_sz = 2 * PG_SZ;
  void *const pt_mem = e820_alloc(dynamic_alloc_sz, false);
  if (pt_mem == NULL) {
    pmode_puts("failed to alloc page table entries\r\n");
    return false;
  }

  void *const pd = pt_mem;
  void *const dm_pt = pt_mem + PG_SZ;
  _pt_zero_pg(pd);
  // Set up 1MB low memory direct map.
  _pt_zero_pg(dm_pt);
  for (uint64_t pg = PG_SZ; pg < (1 * MB); pg += PG_SZ) {
    _pt_map_pg(pd, dm_pt, pg, pg, false);
  }
  // Set up 1GB HHDM.
  for (uint64_t pg = 0; pg < (1 * GB) - KERNEL_MAP_SZ; pg += HUGE_PG_SZ) {
    _pt_map_hugepg(pd, pg + HM_START, pg, true);
  }
  // Set up kernel mapping.
  for (uint64_t pg = 0; pg < KERNEL_MAP_SZ; pg += HUGE_PG_SZ) {
    _pt_map_hugepg(pd, pg + KERNEL_LOAD_ADDR, pg + (uint32_t)kernel_paddr,
                   true);
  }
  enable_paging(pd);

  return e820_augment_bootloader((uint64_t)pt_mem, dynamic_alloc_sz);
}

bool augment_bootloader_text_stack_sections() {
  // Bootloader stack and text regions, respectively.
  return e820_augment_bootloader((uint64_t)&mbr_start - PG_SZ, PG_SZ) &&
         e820_augment_bootloader((uint64_t)&mbr_start, 63 * 0x200);
}

bool check_cpuid_features() {
  struct cpuid_features feat;
  get_cpuid_features(&feat);

  pmode_puts("cpuid features (%edx:%ecx)=");
  pmode_printq(*(uint64_t *)&feat);
  pmode_puts("\r\n");

  if (!feat.pae || !feat.pse) {
    // We depend on PSE (4MB pages) and may depend on PAE later (for
    // physical addresses above 4GB).
    pmode_puts("missing required cpuid features (PAE and PSE)\r\n");
    return false;
  }

  return true;
}

/// Check that HHDM matches low memory for first (almost) megabyte.
void check_paging_setup() {
  /// Note: this memcmp is pretty slow, since the basic memcmp
  /// operator we have goes one byte at a time.
  pmode_puts("Checking that the HHDM matches low memory...\r\n");
  bool direct_map_matches_hhdm =
      !memcmp((void *)PG_SZ, (void *)HM_START + PG_SZ, 1 * MB - PG_SZ);
  if (direct_map_matches_hhdm) {
    pmode_puts("paging is (now) set up correctly\r\n");
  } else {
    pmode_puts("paging not set up correctly\r\n");
  }
}

/// Jump to the kernel! This is not implemented in a regular ASM file
/// despite being fully ASM since it requires a C macro.
__attribute__((naked)) void jump_to_kernel() {
  __asm__ volatile("add %0, %%esp\n\t"
                   "jmp *%1"
                   :
                   : "rm"(HM_START), "rm"(KERNEL_LOAD_ADDR));
}
