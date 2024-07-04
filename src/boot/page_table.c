#include "pmode_print.h"
#include <stdbool.h>
#include <stddef.h>

#define PG_SZ 4096

enum e820_mm_type {
  E820_MM_TYPE_USABLE = 1,
  E820_MM_TYPE_RESERVED,
  E820_MM_TYPE_ACPI_RECLAIMABLE,
  E820_MM_TYPE_ACPI_NVS,
  E820_MM_TYPE_BAD_MEM,

  // Non-standard type, indicates a bootloader type to the kernel.
  E820_MM_TYPE_BOOTLOADER = 6,
};
struct e820_mm_entry {
  uint64_t base;
  uint64_t len;
  // Can't use the enum type since the size is undefined.
  uint32_t type;
  uint32_t acpi_extended_attrs;
};
/// Defined in mem.S.
extern const unsigned e820_mm_max_entries;
extern struct e820_mm_entry e820_mem_map[];

/// Defined in boot.S. Start of text region.
///
/// We only use its address, so its okay to type this void (an
/// incomplete type).
extern const void mbr_start;

static bool _e820_entry_present(struct e820_mm_entry *const ent) {
  // An all-empty entry indicates the end of an array.
  return ent->base || ent->len || ent->type || ent->acpi_extended_attrs;
}

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
  for (ent = e820_mem_map; _e820_entry_present(ent); ++ent) {
    _e820_entry_print(ent);
  }
  pmode_puts("Number of entries: ");
  pmode_printb(ent - e820_mem_map);
  pmode_puts("\r\n");
}

/// Alloc a contiguous region of memory of the given number of pages
/// from usable memory regions.
///
/// \return start of the allocated region, or NULL if such a region
/// was not found
void *_e820_alloc(unsigned num_pg) {
  // Really stupid allocation method for now since this only gets used
  // once: loop one megabyte at a time in the first GB until we find a
  // free area of this size.
  uint64_t len = num_pg * PG_SZ;
  for (uint64_t base = 0x100000; base < 0x40000000; base += 0x100000) {
    // Check that this region completely exists in a usable memory
    // region and doesn't overlap any bad memory regions. We need to
    // check both because the memory regions aren't normalized and may
    // overlap.
    bool is_usable = false;
    bool overlaps_unusable = false;
    struct e820_mm_entry *ent;
    for (ent = e820_mem_map; _e820_entry_present(ent); ++ent) {
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

/// Augment the E820 memory map with a "bootloader" type section.
///
/// \return false if there isn't enough space in the E820 memory map
/// array, true otherwise.
bool _e820_augment_bootloader(uint64_t base, uint64_t len) {
  struct e820_mm_entry *ent;
  for (ent = e820_mem_map; _e820_entry_present(ent); ++ent) {
  }

  if (ent - e820_mem_map + 1 >= e820_mm_max_entries) {
    // Last entry must be a null entry.
    return false;
  }

  // Entry should be zeroed beforehand.
  ent->base = base;
  ent->len = len;
  ent->type = E820_MM_TYPE_BOOTLOADER;
  return true;
}

void _pt_setup_pd(void *pd) {
  // TODO
}
void _pt_setup_dm(void *pd, void *dm_pt) {
  // TODO
}
void _pt_setup_hhdm(void *pd, void *hhdm_pts) {
  // TODO
}
void _pt_enable(void *pd) {
  // TODO
}

bool pt_setup() {
  // Allocate space for page table mapping:
  // - 256 page tables for 1GB HHDM.
  // - 1 page table for 1MB direct map.
  // - 1 page directory.
  void *pt_mem = _e820_alloc(258);
  if (pt_mem == NULL) {
    pmode_puts("failed to alloc page table entries\r\n");
    return false;
  }

  void *page_directory = pt_mem;
  void *dm_page_table = pt_mem + PG_SZ;
  void *hhdm_page_tables = pt_mem + 2 * PG_SZ;
  _pt_setup_pd(page_directory);
  _pt_setup_dm(page_directory, dm_page_table);
  _pt_setup_hhdm(page_directory, hhdm_page_tables);
  _pt_enable(page_directory);

  if ( // Bootloader stack.
      !_e820_augment_bootloader((uint64_t)&mbr_start - 0x1000, 0x1000) ||
      // Bootloader text region.
      !_e820_augment_bootloader((uint64_t)&mbr_start, 63 * 0x200) ||
      // Bootloader page tables.
      !_e820_augment_bootloader((uint64_t)pt_mem, 258 * PG_SZ)) {
    pmode_puts("failed to augment mm with bootloader entries\r\n");
    return false;
  }
  return true;
}
