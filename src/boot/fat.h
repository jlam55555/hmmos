/// @file fat.h
/// @brief FAT32 driver for the bootloader. Only used to locate and
/// load the kernel binary.
///
#include "mbr.h"

struct fat32_desc {
  // TODO
};

struct fat32_file_desc {
  // TODO
};

struct fat32_file_cluster {
  // TODO
};

/// Reads filesystem metadata from partition.
///
/// @param part_desc The partition containing the kernel.
/// @param fs_desc In/out parameter. Will contain the FAT metadata on
///                function exit.
/// @return success
bool fat32_parse_partition(struct mbr_partition_desc const *part_desc,
                           struct fat32_desc *fs_desc);

/// Walks the FAT32 root partition to find the file. Cannot find files
/// in subdirectories.
///
/// @param fs_desc Partition to search
/// @param filename Filename to search for
/// @param file_desc In/out parameter. Will contain the file
///                  information on exit.
/// @return success
bool fat32_find_file(struct fat32_desc const *fs_desc, const char *filename,
                     struct fat32_file_desc *file_desc);

/// Walks the cluster linked list for a particular file. Calls the
/// provided callback on each cluster.
bool fat32_read_file(struct fat32_file_desc const *file_desc,
                     bool (*cb)(struct fat32_file_cluster *));
