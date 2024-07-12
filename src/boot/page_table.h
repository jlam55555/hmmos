#pragma once
/// Some useful definitions used by page_table.c (and maybe the kernel
/// once we have that).

#include "boot_protocol.h"
#include <stdbool.h>
#include <stdint.h>

struct cpuid_features {
  // Features returned in %ecx.
  bool sse3 : 1;                // 0
  bool pclmulqdq : 1;           // 1
  bool dtes64 : 1;              // 2
  bool monitor : 1;             // 3
  bool ds_cpl : 1;              // 4
  bool vmx : 1;                 // 5
  bool smx : 1;                 // 6
  bool eist : 1;                // 7
  bool tm2 : 1;                 // 8
  bool ssse3 : 1;               // 9
  bool cnxt_id : 1;             // 10
  bool sdbg : 1;                // 11
  bool fma : 1;                 // 12
  bool cmxchg16b : 1;           // 13
  bool xtpr_update_control : 1; // 14
  bool pdcm : 1;                // 15
  bool rsv0 : 1;                // 16; reserved
  bool pcid : 1;                // 17
  bool dca : 1;                 // 18
  bool sse4_1 : 1;              // 19
  bool sse4_2 : 1;              // 20
  bool x2apic : 1;              // 21
  bool movbe : 1;               // 22
  bool popcnt : 1;              // 23
  bool tsc_deadline : 1;        // 24
  bool aesni : 1;               // 25
  bool xsave : 1;               // 26
  bool osxsave : 1;             // 27
  bool avx : 1;                 // 28
  bool f16c : 1;                // 29
  bool rdrand : 1;              // 30
  bool rsv1 : 1;                // 31; reserved
  // Features returned in %edx.
  bool fpu : 1;    // 32
  bool vme : 1;    // 33
  bool de : 1;     // 34
  bool pse : 1;    // 35
  bool tsc : 1;    // 36
  bool msr : 1;    // 37
  bool pae : 1;    // 38
  bool mce : 1;    // 39
  bool cx8 : 1;    // 40
  bool apic : 1;   // 41
  bool rsv2 : 1;   // 42; reserved
  bool sep : 1;    // 43
  bool mtrr : 1;   // 44
  bool pge : 1;    // 45
  bool mca : 1;    // 46
  bool cmov : 1;   // 47
  bool pat : 1;    // 48
  bool pse_36 : 1; // 49
  bool psn : 1;    // 50
  bool clfsh : 1;  // 51
  bool rsv3 : 1;   // 52; reserved
  bool ds : 1;     // 53
  bool acpi : 1;   // 54
  bool mmx : 1;    // 55
  bool fxsr : 1;   // 56
  bool sse : 1;    // 57
  bool sse2 : 1;   // 58
  bool ss : 1;     // 59
  bool htt : 1;    // 60
  bool tm : 1;     // 61
  bool rsv4 : 1;   // 62; reserved
  bool pbe : 1;    // 63
} __attribute__((packed));

_Static_assert(sizeof(struct cpuid_features) == 8,
               "cpuid_features should be 64 bits");

struct page_directory_entry {
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

struct page_directory_entry_4mb {
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

struct page_table_entry {
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

_Static_assert(sizeof(struct page_directory_entry) == 4,
               "sizeof(pde) should be 4");
_Static_assert(sizeof(struct page_directory_entry_4mb) == 4,
               "sizeof(pde) should be 4");
_Static_assert(sizeof(struct page_table_entry) == 4, "sizeof(pte) should be 4");

/// Defined in mem.S.
extern const unsigned e820_mm_max_entries;
extern struct e820_mm_entry e820_mem_map[];

/// Defined in boot.S. Start of text region.
extern void mbr_start();

/// Defined in stage2.S.
extern void enable_paging(struct page_directory_entry *);
extern void get_cpuid_features(struct cpuid_features *);

/// Alloc a contiguous region of memory of the given size from usable
/// memory regions.
///
/// The returned physical memory region is guaranteed to be at least
/// page-aligned (hugepage-aligned if hugepg_align is true).
///
/// \return start of the allocated region, or NULL if such a region
/// was not found
void *e820_alloc(unsigned len, bool hugepg_align);

/// Augment the E820 memory map with a "bootloader" type section.
///
/// \return false if there isn't enough space in the E820 memory map
/// array, true otherwise.
bool e820_augment_bootloader(uint64_t base, uint64_t len);

/// Helper function to set up e820 map entries for the bootloader text
/// and stack regions.
///
/// \return true iff success
bool augment_bootloader_text_stack_sections();

/// Allocate space for page table mapping:
/// - 1 page directory (will use 4MB pages for HHDM).
/// - 1 page table for 1MB direct map (can't use 4MB page since
///   we don't want to map the null page).
///
/// This can probably fit within the text region of the bootloader
/// (since we have ~32KB) but we may need to allocate more pages in
/// the future, e.g. if using 3-level (PAE) paging. Let's leave this
/// dynamically allocated for now.
///
/// \param kernel_paddr Kernel offset in physical memory.
///
/// \return true iff success
bool pt_setup(void *kernel_paddr);
