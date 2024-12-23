#include "fat.h"
#include "boot_protocol.h"
#include "console.h"
#include "libc_minimal.h"
#include "mbr.h"
#include "page_table.h"
#include "perf.h"
#include <assert.h>

// defined in stage2.S
extern void copy_bytes(void *mem_addr, void *disk_addr, uint32_t len);

struct _fat32_vbr {
  // Note: not a jmp instruction in HmmOS, as all the bootloader code
  // is in the sectors following the MBR.
  uint32_t jmp_instr : 24;
  const char oem_name[8];

  struct extended_bios_parameter_block {
    // Note: All "sectors" indicate logical sectors here of size \a
    // bytes_per_sector.

    // BPB DOS 2.0
    uint16_t bytes_per_sector;
    uint8_t sectors_per_cluster;
    uint16_t reserved_sectors;
    uint8_t fats;
    uint16_t max_root_dir_entries;
    uint16_t total_sectors;
    uint8_t media_desc;
    uint16_t sectors_per_fat;

    // BPB DOS 3. extensions2 -- assumes \a total_sectors == 0.
    uint16_t sectors_per_track;
    uint16_t heads_per_disk;
    uint32_t hidden_sectors;
    uint32_t total_sectors2;

    // FAT32 Extended BPB
    uint32_t sectors_per_fat2;
    uint16_t drive_mirror_flags;
    uint16_t version;
    uint32_t root_dir_start_cluster;
    uint16_t fs_information_start_sector;
    uint16_t backup_start_sector;
    uint8_t rsvd0[12];
    uint8_t physical_drive_number;
    uint8_t rsvd1;
    uint8_t extended_boot_sig;
    uint32_t volume_id;
    const char volume_label[11];
    const char fs_type[8];
  } __attribute__((packed)) ebpb;

  uint8_t filler[512 - 3 - 90];

  uint8_t physical_drive_no;
  const uint8_t boot_signature[2];
} __attribute__((packed));
static_assert(sizeof(struct _fat32_vbr) == 512, "wrong VBR size");

struct _fat32_dirent {
  const char short_filename[8];
  const char short_fileext[3];

  struct {
    uint8_t readonly : 1;
    uint8_t hidden : 1;
    uint8_t system : 1;
    uint8_t volume_label : 1;
    uint8_t subdir : 1;
    uint8_t archive : 1;
    uint8_t device : 1;
    uint8_t rsvd : 1;
  } __attribute__((packed)) attr;

  uint8_t vfat_case;
  uint8_t created_time_fine;
  uint16_t created_time;
  uint16_t created_date;
  uint16_t access_date;
  uint16_t cluster_hi;
  uint16_t modified_time;
  uint16_t modified_date;
  uint16_t cluster_lo;
  uint32_t file_sz_bytes;
} __attribute__((packed));
static_assert(sizeof(struct _fat32_dirent) == 32, "wrong FAT32 dirent size");

bool fat32_init_desc(struct fat32_desc *fs_desc) {
  // We only need to copy one logical sector at a time (VBR and FAT
  // sectors). We expect the FAT32 logical sector size to be either
  // 512 or 4096 bytes.
  size_t max_logical_sector_sz = 4096;

  void *arena_start = e820_alloc(max_logical_sector_sz, false);
  if (unlikely(arena_start == NULL)) {
    return false;
  }

  if (unlikely(!e820_augment_bootloader((uint64_t)(uint32_t)arena_start,
                                        max_logical_sector_sz,
                                        E820_MM_TYPE_BOOTLOADER_RECLAIMABLE))) {
    return false;
  }

  // Arena must be sector-aligned and at least one sector long.
  assert(((int)arena_start & (MBR_SECTOR_SZ - 1)) == 0);

  fs_desc->buf = arena_start;
  fs_desc->buf_sz = max_logical_sector_sz;
  return true;
}

bool fat32_parse_partition(struct mbr_partition_desc const *part_desc,
                           struct fat32_desc *fs_desc) {
  // Parse VBR.
  uint64_t partition_start_bytes =
      (uint64_t)part_desc->first_sector_lba * MBR_SECTOR_SZ;
  copy_bytes(fs_desc->buf, (void *)(uint32_t)partition_start_bytes,
             MBR_SECTOR_SZ);

  struct _fat32_vbr *vbr = (struct _fat32_vbr *)fs_desc->buf;

  // This should be zero for FAT32; the total sectors should be
  // specified in \a total_sectors2.
  assert(vbr->ebpb.total_sectors == 0);

  // Extended boot signature should be 0x28 or 0x29.
  assert(vbr->ebpb.extended_boot_sig == 0x28 ||
         vbr->ebpb.extended_boot_sig == 0x29);

  // VBR boot signature.
  assert(*(uint16_t *)vbr->boot_signature == 0xAA55);

  // When we copy the kernel using the BIOS function, we assume
  // addresses are 512-byte (MBR sector) aligned.
  assert(vbr->ebpb.bytes_per_sector >= MBR_SECTOR_SZ);

  // We will copy one logical sector into \a fs_desc->buf when
  // traversing the root directory table, so make sure the buffer is
  // large enough.
  assert(vbr->ebpb.bytes_per_sector <= fs_desc->buf_sz);

  fs_desc->bytes_per_sector = vbr->ebpb.bytes_per_sector;
  fs_desc->sectors_per_cluster = vbr->ebpb.sectors_per_cluster;

  // Compute the starts (logical byte offsets on the disk) of the FAT
  // and the data region.
  uint64_t reserved_sectors = vbr->ebpb.reserved_sectors;
  uint64_t fats = vbr->ebpb.fats;
  uint64_t sectors_per_fat = vbr->ebpb.sectors_per_fat2;

  fs_desc->fat_offset_bytes =
      partition_start_bytes + reserved_sectors * fs_desc->bytes_per_sector;
  fs_desc->data_region_offset_bytes =
      fs_desc->fat_offset_bytes +
      fats * sectors_per_fat * fs_desc->bytes_per_sector;

  fs_desc->root_dir_start_cluster = vbr->ebpb.root_dir_start_cluster;

#ifdef DEBUG
  // Longest str in VBR + 1 (for null char).
  char buf[13];

  memset(buf, '\0', sizeof buf);
  memcpy(buf, vbr->oem_name, sizeof vbr->oem_name);
  console_puts("OEM name: ");
  console_puts(buf);

  if (vbr->ebpb.extended_boot_sig == 0x29) {
    // These fields aren't set if the extended boot signature are 0x28.
    memset(buf, '\0', sizeof buf);
    memcpy(buf, vbr->ebpb.volume_label, sizeof vbr->ebpb.volume_label);
    console_puts("\r\nVolume label: ");
    console_puts(buf);

    memset(buf, '\0', sizeof buf);
    memcpy(buf, vbr->ebpb.fs_type, sizeof vbr->ebpb.fs_type);
    console_puts("\r\nFilesystem type: ");
    console_puts(buf);
  }

  console_puts("\r\nbytes_per_sector=");
  console_printl(fs_desc->bytes_per_sector);
  console_puts(" sectors_per_cluster=");
  console_printl(fs_desc->sectors_per_cluster);
  console_puts(" reserved_sectors=");
  console_printl(reserved_sectors);
  console_puts(" fats=");
  console_printl(fats);
  console_puts(" sectors_per_fat=");
  console_printl(sectors_per_fat);
  console_puts(" root_dir_start_cluster=");
  console_printl(fs_desc->root_dir_start_cluster);
  console_puts("\r\n");
#endif

  return true;
}

static uint32_t _fat32_mask_entry(uint32_t cluster) {
  return cluster & 0x0FFFFFFFU;
}

static bool _fat32_is_last_cluster_in_file(uint32_t cluster) {
  return _fat32_mask_entry(cluster) >= 0x0FFFFFF7U;
}

static uint64_t
_fat32_cluster_data_offset_bytes(struct fat32_desc const *fs_desc,
                                 uint32_t cluster) {
  // -2 since cluster 2 is the first cluster in the data region.
  return fs_desc->data_region_offset_bytes + (_fat32_mask_entry(cluster) - 2) *
                                                 fs_desc->sectors_per_cluster *
                                                 fs_desc->bytes_per_sector;
}

static bool _fat32_find_file(struct fat32_desc const *fs_desc,
                             const char *filename,
                             struct fat32_file_desc *file_desc,
                             uint32_t cur_root_dir_cluster) {
  // Each FAT entry/dirent is 4 bytes.
  uint32_t fat_entries_per_sector = fs_desc->bytes_per_sector / 4;

  // Load root directory table, one sector at a time.
  for (uint8_t sector = 0; sector < fs_desc->sectors_per_cluster; ++sector) {
    copy_bytes(fs_desc->buf,
               (void *)(uint32_t)(_fat32_cluster_data_offset_bytes(
                                      fs_desc, cur_root_dir_cluster) +
                                  sector * fs_desc->bytes_per_sector),
               fs_desc->bytes_per_sector);

    struct _fat32_dirent *dirent_start = (struct _fat32_dirent *)fs_desc->buf;
    for (struct _fat32_dirent *it = dirent_start;
         it < dirent_start + fat_entries_per_sector; ++it) {
      switch (it->short_filename[0]) {
      case '\0':
        // No more directory entries.
        return false;
      case (char)0xE5:
        // Deleted file.
        continue;
      }

      // Note: VFAT is not supported.
      if (memcmp(filename, it->short_filename, 11) == 0) {
        file_desc->fs = fs_desc;
        file_desc->start_cluster =
            (((uint32_t)it->cluster_hi) << 2) | it->cluster_lo;
        file_desc->file_sz_bytes = it->file_sz_bytes;
        return true;
      }
    }
  }

  // We didn't see anything in this cluster, load the FAT to find the
  // next cluster and recurse.
  copy_bytes(
      fs_desc->buf,
      (void *)(uint32_t)(fs_desc->fat_offset_bytes +
                         (cur_root_dir_cluster / fat_entries_per_sector) *
                             fs_desc->bytes_per_sector),
      fs_desc->bytes_per_sector);
  uint32_t next_cluster =
      ((uint32_t *)fs_desc->buf)[cur_root_dir_cluster % fat_entries_per_sector];
  // We shouldn't reach this; instead we should see an EOF dirent
  // marker.
  assert(!_fat32_is_last_cluster_in_file(next_cluster));
  return _fat32_find_file(fs_desc, filename, file_desc,
                          _fat32_mask_entry(next_cluster));
}

bool fat32_find_file(struct fat32_desc const *fs_desc, const char *filename,
                     struct fat32_file_desc *file_desc) {
  // \a filename should be in 8.3 format: an 11-char string with a
  // space-padded filename (8 chars) and extension (3 chars). I'm lazy
  // and didn't implement strlen in the bootloader so this will
  // suffice.
  assert(filename[11] == '\0');
  return _fat32_find_file(fs_desc, filename, file_desc,
                          fs_desc->root_dir_start_cluster);
}

bool fat32_read_file(void *buf, struct fat32_file_desc const *file_desc) {
  uint32_t cluster = file_desc->start_cluster;
  uint32_t remaining_bytes = file_desc->file_sz_bytes;

  uint32_t last_fat_sector = -1;

  do {
    uint32_t bytes_to_read =
        file_desc->fs->sectors_per_cluster * file_desc->fs->bytes_per_sector;
    if (bytes_to_read > remaining_bytes) {
      bytes_to_read = remaining_bytes;
    }

    const uint32_t bytes_read = file_desc->file_sz_bytes - remaining_bytes;
    copy_bytes(buf + bytes_read,
               (void *)(uint32_t)_fat32_cluster_data_offset_bytes(file_desc->fs,
                                                                  cluster),
               bytes_to_read);
    remaining_bytes -= bytes_to_read;

    // Advance cluster. Technically we can skip this if \a
    // remaining_bytes == 0, but let's check that we get the
    // end-of-file linked list marker in the FAT.
    const uint32_t fat_entries_per_sector = file_desc->fs->bytes_per_sector / 4;
    const uint32_t fat_sector = cluster / fat_entries_per_sector;
    if (fat_sector != last_fat_sector) {
      // Need to load a new FAT sector into fs->buf.
      copy_bytes(file_desc->fs->buf,
                 (void *)(uint32_t)file_desc->fs->fat_offset_bytes +
                     fat_sector * file_desc->fs->bytes_per_sector,
                 file_desc->fs->bytes_per_sector);
      last_fat_sector = fat_sector;
    }
    cluster =
        ((uint32_t *)file_desc->fs->buf)[cluster % fat_entries_per_sector];
  } while (!_fat32_is_last_cluster_in_file(cluster));

  assert(remaining_bytes == 0);
  return true;
}
