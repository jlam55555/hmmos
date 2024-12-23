#pragma once

/// \file
/// \brief Master Boot Record definitions

#include "page_table.h"
#include <assert.h>
#include <stdint.h>

/// MBR assumes 512-byte sectors.
#define MBR_SECTOR_SZ 0x200U

/// 16-byte sector descriptor in MBR partition table.
struct mbr_partition_desc {
  uint8_t drive_attrs;
  uint32_t first_sector_chs : 24;
  uint8_t partition_type;
  uint32_t last_sector_chs : 24;
  uint32_t first_sector_lba;
  uint32_t sector_count;
} __attribute__((packed));

_Static_assert(sizeof(struct mbr_partition_desc) == 16,
               "mbr_partition_desc should be 16 bytes");
