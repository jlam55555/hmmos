#pragma once

/// \file
/// \brief The runtime interface between the bootloader and kernel.
///
/// The bootloader can share data with the kernel (e.g., physical
/// memory map) and the kernel can request bootloader functionality
/// (e.g., video mode) via this interface. Design heavily inspired by
/// the Limine boot protocol.
///
/// See the BP_REQ() macro for the kernel-level interface for
/// declaring kernel requests.

#include "memdefs.h"
#include <stdbool.h>
#include <stdint.h>

#ifndef KERNEL_LOAD_ADDR
/// \brief Virtual address to load kernel text
///
/// Save 32MB at the top of the virtual address space for the kernel
/// load address. This also dictates the maximum size of the kernel
/// binary, so this will need to be overridden (and the linker script
/// modified) if the kernel binary size exceeds this.
///
/// This is a compromise due to the limited size of the 32-bit address
/// space, and the fact that we want the kernel to be loaded at a
/// fixed address (i.e., nothing fancy like PIE). The larger this
/// section is, the more space we take away from the 1GB HHDM, so the
/// kernel will be able to address less memory without creating its
/// own memory mappings.
///
/// TODO: This could be a kernel request, although I'm not sure how
/// useful that would be. The kernel size is known at compile-time so
/// this should mostly be fine.
#define KERNEL_LOAD_ADDR (4 * GB - 32 * MB)
#endif
#define KERNEL_MAP_SZ (4 * GB - KERNEL_LOAD_ADDR)

_Static_assert(HUGEPG_ALIGNED(KERNEL_LOAD_ADDR),
               "KERNEL_LOAD_ADDR must be hugepage-aligned");

enum e820_mm_type {
  E820_MM_TYPE_USABLE = 1,
  E820_MM_TYPE_RESERVED,
  E820_MM_TYPE_ACPI_RECLAIMABLE,
  E820_MM_TYPE_ACPI_NVS,
  E820_MM_TYPE_BAD_MEM,

  /// Bootloader-allocated memory. Includes bootloader stack, and page
  /// tables, and kernel text region, which are all used by the HmmOS
  /// kernel and thus cannot be reclaimed. (This is different than the
  /// Limine boot protocol which assumes the kernel will set up its
  /// own stack and text regions, and thus those regions are
  /// reclaimable.)
  E820_MM_TYPE_BOOTLOADER = 6,

  /// Bootloader-allocated memory that can be reclaimed by the
  /// kernel. Includes dynamic memory allocated by the kernel and
  /// bootloader text regions. Can be treated as usable memory by the
  /// kernel.
  E820_MM_TYPE_BOOTLOADER_RECLAIMABLE = 7,
};

struct e820_mm_entry {
  uint64_t base;
  uint64_t len;
  // Can't use the enum type since the size is undefined.
  uint32_t type;
  uint32_t acpi_extended_attrs;

#ifdef __cplusplus
  bool operator<=>(const e820_mm_entry &) const = default;
#endif
};

static inline bool e820_entry_present(const struct e820_mm_entry *ent) {
  // An all-empty entry indicates the end of an array.
  return ent->base || ent->len || ent->type || ent->acpi_extended_attrs;
}

/// \brief Randomly generated. Used to identify bootloader requests in
/// the kernel binary.
///
#define BP_REQ_MAGIC 0xF7438B7CA1676C21ULL

/// \brief The bootloader will scan for any requests. This must be
/// aligned to make the search faster for the bootloader.
#define BP_REQ_ALIGN 8

enum bp_req_id {
  /// As a sanity check, a zero reqid is treated as an error. Start at
  /// index 1.
  BP_REQID_MEMORY_MAP = 1,
};

/// Helpers for BP_REQ, since (afaik) the preprocessor doesn't have
/// functionality to change the case of tokens.
#define _BP_REQ_MEMORY_MAP bp_req_memory_map

struct bp_req_header {
  uint64_t magic;
  uint32_t req_id;
} __attribute__((packed));

struct bp_req_memory_map {
  struct bp_req_header hdr;
  struct e820_mm_entry *memory_map;
} __attribute__((packed, aligned(BP_REQ_ALIGN)));

/// \brief Kernel interface for defining a bootloader request.

/// The request object needs to be a global variable and not optimized
/// out of the binary.
///
/// Example:
/// ```
/// static volatile const BP_REQ(MEMORY_MAP, _mem_map_req);
/// ```
/// This is akin to defining `struct bp_req_memory_map _mem_map_req`
/// with the required magic variables and request ID.
///
/// \param REQ_ID the name of the request. It should be all caps and
///     not include the BP_REQID/bp_req prefix.
/// \param VAR the name of the struct we're declaring.
#define BP_REQ(REQ_ID, VAR)                                                    \
  struct _BP_REQ_##REQ_ID VAR = {                                              \
      .hdr = {.magic = BP_REQ_MAGIC, .req_id = BP_REQID_##REQ_ID}};
