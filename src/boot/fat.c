#include "fat.h"

bool fat32_parse_partition(struct mbr_partition_desc const *part_desc,
                           struct fat32_desc *fs_desc) {
  return false;
}

bool fat32_find_file(struct fat32_desc const *fs_desc, const char *filename,
                     struct fat32_file_desc *file_desc) {
  return false;
}

bool fat32_read_file(struct fat32_file_desc const *file_desc,
                     bool (*cb)(struct fat32_file_cluster *)) {
  return false;
}
