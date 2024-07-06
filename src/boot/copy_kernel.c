#include "boot_protocol.h"
#include "mbr.h"
#include "page_table.h"
#include "perf.h"
#include "pmode_print.h"
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

extern void copy_bytes(void *mem_addr, void *disk_addr, uint32_t len);

static struct mbr_partition_desc *const _mbr_partitions = (void *)0x7DBE;

void read_mbr_partitions() {
  struct mbr_partition_desc const *part = _mbr_partitions;
  for (int i = 0; i < 4; ++i, ++part) {
    pmode_puts("Partition ");
    pmode_printb(i + 1); // 1-indexed
    pmode_puts(": ");
    if (!part->partition_type) {
      pmode_puts("empty");
    } else {
      // We don't care about CHS so let's not print it here.
      pmode_puts("attrs=");
      pmode_printb(part->drive_attrs);
      pmode_puts(" type=");
      pmode_printb(part->partition_type);
      pmode_puts(" start_lba=");
      pmode_printl(part->first_sector_lba);
      pmode_puts(" sectors=");
      pmode_printl(part->sector_count);
    }
    pmode_puts("\r\n");
  }
}

/// Scan the kernel for the magic bytes.
static void _fulfill_boot_protocol_requests(void *kernel_addr,
                                            size_t kernel_len) {
  // `struct bp_request_header` has 8-byte alignment so we can search
  // a little more efficiently.
  for (void *kernel_end = kernel_addr + kernel_len, *needle = kernel_addr;
       needle < kernel_end; needle += BP_REQ_ALIGN) {
    if (likely(*(uint64_t *)needle != BP_REQ_MAGIC)) {
      continue;
    }

    struct bp_req_header *req_hdr = needle;
    pmode_puts("Found CPU request with type ");
    pmode_printl(req_hdr->req_id);
    pmode_puts("\r\n");

    switch (req_hdr->req_id) {
    case BP_REQID_MEMORY_MAP: {
      struct bp_req_memory_map *req = needle;
      req->memory_map = e820_mem_map;
      break;
    }
    default:
      pmode_puts("Invalid req_id, skipping...\r\n");
    }
  }
}

/// Copies the kernel to memory.
///
/// \return The physical address that the kernel was copied to on
/// success, otherwise NULL.
void *copy_kernel() {
  struct mbr_partition_desc const *part = _mbr_partitions;
  for (int i = 0; i < 4; ++i, ++part) {
    // For now, assume the correct partition has type 0xFF. This is
    // set by install_bootloader.py.
    if (part->partition_type == 0xFF) {
      const uint32_t kernel_len = part->sector_count * SECTOR_SZ;

      // Kernel is larger than KERNEL_LOAD_ADDR. This constant needs
      // to be configured higher.
      assert(kernel_len <= 4 * GB - KERNEL_LOAD_ADDR);

      pmode_puts("Allocating memory for the kernel...\r\n");
      void *kernel_paddr = e820_alloc(kernel_len, true);
      if (!kernel_paddr) {
        return kernel_paddr;
      }

      e820_augment_bootloader((uint64_t)kernel_paddr, kernel_len);

      copy_bytes(kernel_paddr, (void *)(part->first_sector_lba * SECTOR_SZ),
                 kernel_len);
      pmode_puts("Copied the kernel to memory.\r\n");

      _fulfill_boot_protocol_requests(kernel_paddr, kernel_len);
      pmode_puts("Fulfilled kernel boot protocol requests.\r\n");

      return kernel_paddr;
    }
  }
  pmode_puts("no kernel partition found.\r\n");
  return NULL;
}
