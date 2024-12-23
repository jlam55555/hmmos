/// \file fat.h
/// \brief FAT32 driver for the bootloader. Only used to locate and
/// load the kernel binary.
///
/// \see https://en.wikipedia.org/wiki/Design_of_the_FAT_file_system
///
#include "mbr.h"
#include "stddef.h"

/// Condensed version of the useful information from the VBR that are
/// useful for the bootloader.
struct fat32_desc {
  /// In-memory copy of FAT metadata.
  ///
  /// This is only used for the bootloader's FAT parsing and thus
  /// should live in bootloader-reclaimable memory, and is allocated
  /// using \ref fat32_init_desc(). It must be sector-aligned and at
  /// least one (logical FAT) sector in size. For the bootloader's
  /// purposes, we only load one sector of FAT metadata at a time.
  void *buf;
  size_t buf_sz;

  uint16_t bytes_per_sector;
  uint8_t sectors_per_cluster;

  // Offset in bytes from start of disk.
  uint64_t fat_offset_bytes;
  uint64_t data_region_offset_bytes;

  uint32_t root_dir_start_cluster;
};

struct fat32_file_desc {
  struct fat32_desc const *fs;
  uint32_t start_cluster;
  uint32_t file_sz_bytes;
};

/// Initiate fat32_desc object.
bool fat32_init_desc(struct fat32_desc *fs_desc);

/// Reads filesystem metadata from partition.
///
/// \param part_desc The partition containing the kernel.
/// \param fs_desc In/out parameter. Will contain the FAT metadata on
///                function exit.
/// \return success
bool fat32_parse_partition(struct mbr_partition_desc const *part_desc,
                           struct fat32_desc *fs_desc);

/// Walks the FAT32 root directory to find the file. Cannot find files
/// in subdirectories.
///
/// \param fs_desc Partition to search
/// \param filename Filename to search for (in 8.3 format)
/// \param file_desc In/out parameter. Will contain the file
///                  information on exit.
/// \return success
bool fat32_find_file(struct fat32_desc const *fs_desc, const char *filename,
                     struct fat32_file_desc *file_desc);

/// Reads the file pointed to by \a file_desc and writes it to \a
/// buf. Assumes \a buf is allocated large enough to contain the
/// entire file.
bool fat32_read_file(void *buf, struct fat32_file_desc const *file_desc);
