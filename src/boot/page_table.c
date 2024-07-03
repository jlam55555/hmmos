#include "pmode_print.h"
#include <stdbool.h>

enum e820_mm_type {
  E820_MM_TYPE_USABLE = 1,
  E820_MM_TYPE_RESERVED,
  E820_MM_TYPE_ACPI_RECLAIMABLE,
  E820_MM_TYPE_ACPI_NVS,
  E820_MM_TYPE_BAD_MEM,
};
struct e820_mm_entry {
  uint64_t base;
  uint64_t len;
  // Can't use the enum type since the size is undefined.
  uint32_t type;
  uint32_t acpi_extended_attrs;
};
/// Defined in mem.S.
#define E820_MM_MAX_ENTRIES 16
extern struct e820_mm_entry e820_mem_map[E820_MM_MAX_ENTRIES];

static bool _e820_entry_present(struct e820_mm_entry *ent) {
  // An all-empty entry indicates the end of an array.
  return ent->base || ent->len || ent->type || ent->acpi_extended_attrs;
}

/// Simplified printing function. Ignore ACPI attributes for now.
static void _e820_entry_print(struct e820_mm_entry *ent) {
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
  int i;
  struct e820_mm_entry *ent;
  pmode_puts("E820 memory map:\r\n");
  for (ent = e820_mem_map; _e820_entry_present(ent); ++ent) {
    _e820_entry_print(ent);
  }
  pmode_puts("Number of entries: ");
  pmode_printb(ent - e820_mem_map);
  pmode_puts("\r\n");
}

void pt_setup() {
  // TODO: Fixup memory map? Sort and page-align sections, then remove
  // overlapping unusable regions from usable regions.
  //
  // TODO: Augment memory map with bootloader memory regions: stack
  // (4KB below 0x7C00), text/data region (63 x 512B sectors starting
  // at 0x7C00), and page table regions (~1MB somewhere dynamically
  // allocated).
  //
  // TODO: Actually set up page table mapping (HHDM and 1MB direct
  // mapping).
}
