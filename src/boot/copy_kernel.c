#include "boot_protocol.h"
#include "console.h"
#include "fat.h"
#include "mbr.h"
#include "page_table.h"
#include "perf.h"
#include <libc_minimal.h>
#include <stdbool.h>
#include <stdint.h>

// defined in stage2.S
extern void copy_bytes(void *mem_addr, void *disk_addr, uint32_t len);

static struct mbr_partition_desc *const _mbr_partitions = (void *)0x7DBE;

void read_mbr_partitions() {
  struct mbr_partition_desc const *part = _mbr_partitions;
  for (int i = 0; i < 4; ++i, ++part) {
    console_puts("Partition ");
    console_printb(i + 1); // 1-indexed
    console_puts(": ");
    if (!part->partition_type) {
      console_puts("empty");
    } else {
      // We don't care about CHS so let's not print it here.
      console_puts("attrs=");
      console_printb(part->drive_attrs);
      console_puts(" type=");
      console_printb(part->partition_type);
      console_puts(" start_lba=");
      console_printl(part->first_sector_lba);
      console_puts(" sectors=");
      console_printl(part->sector_count);
    }
    console_puts("\r\n");
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
    console_puts("Found CPU request with type ");
    console_printl(req_hdr->req_id);
    console_puts("\r\n");

    switch (req_hdr->req_id) {
    case BP_REQID_MEMORY_MAP: {
      struct bp_req_memory_map *req = needle;
      req->memory_map = e820_mem_map;
      break;
    }
    default:
      console_puts("Invalid req_id, skipping...\r\n");
    }
  }
}

void *copy_kernel() {
  struct mbr_partition_desc const *part_desc = NULL, *it = _mbr_partitions;
  for (int i = 0; i < 4; ++i, ++it) {
    if (it->partition_type == /*FAT32, LBA*/ 0x0C &&
        it->drive_attrs == /* active/bootable */ 0x80) {
      part_desc = it;
      break;
    }
  }
  if (unlikely(part_desc == NULL)) {
    console_puts("Couldn't locate FAT32 partition\r\n");
    return NULL;
  }

  struct fat32_desc fs_desc;
  if (unlikely(!fat32_init_desc(&fs_desc))) {
    console_puts("Couldn't initialize fs_desc\r\n");
    return NULL;
  }

  if (unlikely(!fat32_parse_partition(part_desc, &fs_desc))) {
    console_puts("Couldn't parse FAT32 partition\r\n");
    return NULL;
  }

  struct fat32_file_desc kernel_file_desc;
  if (unlikely(!fat32_find_file(&fs_desc, "KERNEL  BIN", &kernel_file_desc))) {
    console_puts("Couldn't find /KERNEL.BIN in FAT32 partition\r\n");
    return NULL;
  }

  // Kernel is larger than KERNEL_LOAD_ADDR. This constant needs
  // to be configured higher.
  size_t kernel_len = kernel_file_desc.file_sz_bytes;
  assert(kernel_len <= 4 * GB - KERNEL_LOAD_ADDR);
  console_puts("Allocating memory for the kernel...\r\n");
  void *kernel_paddr = e820_alloc(kernel_len, true);
  if (unlikely(kernel_paddr == NULL)) {
    return kernel_paddr;
  }
  e820_augment_bootloader((size_t)kernel_paddr, kernel_len,
                          E820_MM_TYPE_BOOTLOADER);

  if (unlikely(!fat32_read_file(kernel_paddr, &kernel_file_desc))) {
    console_puts("Couldn't read /KERNEL.BIN\r\n");
    return NULL;
  }

  _fulfill_boot_protocol_requests(kernel_paddr, kernel_len);
  console_puts("Fulfilled kernel boot protocol requests.\r\n");

  return kernel_paddr;
}
