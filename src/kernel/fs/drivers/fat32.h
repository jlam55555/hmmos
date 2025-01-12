#pragma once

/// \file fat32.h
/// \brief FAT32 filesystem driver
///
/// TODO: make this thread-safe

#include "drivers/ahci.h"
#include "memdefs.h"
#include "nonstd/libc.h"
#include "nonstd/memory.h"
#include "nonstd/string_view.h"
#include <functional>
#include <optional>
#include <sys/types.h>

namespace fs::fat32 {

/// Partition descriptor in the MBR (at offset 0x01BE).
///
struct MBRPartition {
  uint8_t drive_attrs;
  uint32_t first_sector_chs : 24;
  uint8_t partition_type;
  uint32_t last_sector_chs : 24;
  uint32_t first_sector_lba;
  uint32_t sector_count;
} __attribute__((packed));
static_assert(sizeof(MBRPartition) == 16);

struct VBR;
struct DirectoryEntry;

/// Interface for interacting with FAT32 filesystem.
///
class Filesystem {
public:
  /// Returns the boot FAT32 partition.
  ///
  /// This reads the MBR, duplicating some work from the bootloader.
  ///
  static std::optional<MBRPartition> find_boot_part();

  /// Initialize a FAT32 filesystem object for the given partition.
  ///
  static Filesystem from_partition(MBRPartition &boot_part);

  struct FileDescriptor {
    uint32_t start_cluster;
    uint32_t file_sz_bytes;
    bool is_file;
    /// Null-terminated "normal" filename
    char name[13];
  };

  // Public interface. This a low-level interface that will be be used
  // by the VFS interface eventually (which will actually provide the
  // syscall interface). For now I've modelled the functions after the
  // syscall interface.

  /// Lookup a file descriptor by relative path (starting from \a
  /// dir).
  ///
  std::optional<FileDescriptor> lookup_file(const FileDescriptor &dir,
                                            nonstd::string_view path);

  /// Read bytes at file offset \a off into the buffer \a buf, up to
  /// the size of \a buf.
  ///
  /// Unlike the syscall, this is stateless and does not maintain a
  /// file pointer. The VFS is expected to maintain file state.
  ///
  /// Unlike the syscall, this uses the FAT32-specific file
  /// descriptor, not the file descriptor type exposed by the VFS.
  ///
  /// \return On success, the number of bytes read (zero indicates
  ///         EOF). On error, -1 is returned.
  ssize_t read(const FileDescriptor &fd, size_t off, std::span<std::byte> buf);

  /// Returns directory entries in \a out, up to the size of \a buf.
  ///
  /// Unlike the syscall, this uses the FAT32-specific file
  /// descriptor, not the file descriptor or inode type exposed by the
  /// VFS.
  ///
  /// \return On success, the number of entries is returned. On end of
  ///         directory, 0 is returned. On error, -1 is returned.
  ssize_t getdents(const FileDescriptor &fd, std::span<FileDescriptor> buf);

  /// Returns the root file descriptor.
  ///
  FileDescriptor root_fd() const {
    return {.start_cluster = root_dir_start_cluster,
            .file_sz_bytes = 0,
            .is_file = false,
            .name = ""};
  }

  /// For debugging -- recursively dumps a filetree at the given path.
  ///
  void dump_tree(const FileDescriptor &fd);

private:
  static constexpr unsigned fat_entries_per_sector = 512 / sizeof(uint32_t);

  Filesystem(const VBR &vbr, const MBRPartition &part);

  /// Helper function for iterating directories.
  ///
  /// std::function isn't the best for performance; this can be easily
  /// changed to a function template parameter if wanted.
  ///
  /// This is NOT reentrant-safe, i.e. \ref cb() may not call \ref
  /// iterate_dir().
  ///
  /// \param dir_cluster Start cluster of parent directory
  /// \param cb Callback to call on each directory entry. If it
  ///           returns true, stop iterating the directory.
  void iterate_dir(uint32_t dir_cluster,
                   std::function<bool(const DirectoryEntry *)> cb);

  /// Find a file/subdir in dir using \ref iterate_dir().
  ///
  /// \param path_component A single component of a path (i.e., no /)
  std::optional<FileDescriptor>
  find_file_in_dir(uint32_t dir_cluster, nonstd::string_view path_component);

  /// Returns the LBA of the sector containing the FAT entry for \a cluster.
  ///
  uint32_t get_fat_sector_for_cluster(uint32_t cluster);

  /// Updates the data cluster cache to contain the data from the
  /// given cluster. The last read cluster is cached to reduce
  /// redundant disk reads.
  void read_cluster_to_data_cache(uint32_t cluster);

  /// Returns the next cluster in the linked list by reading the FAT.
  ///
  /// This uses the cached FAT and will avoid a disk read if the
  /// necessary FAT sector was previously loaded.
  ///
  /// Assumes we've already checked that the file does not end,
  /// i.e. the next cluster is expected to not be EOF.
  uint32_t advance_cluster(uint32_t cur_cluster);

  const uint16_t bytes_per_sector;
  const uint8_t sectors_per_cluster;
  const unsigned dir_entries_per_cluster;
  const uint32_t fat_offset_lba;
  const uint32_t data_region_offset_lba;
  const uint32_t root_dir_start_cluster;

  /// 1-sector scratch space for storing the last-accessed FAT sector.
  /// Must be sector-aligned. Should only be used by \ref
  /// advance_cluster().
  const nonstd::unique_ptr<std::byte> fat_cache;

  /// LBA (sector linear address from start of disk) of \ref
  /// fat_cache.
  uint32_t fat_cache_lba = -1;

  /// 1-cluster scratch space for storing the last-accessed (directory
  /// or file) data cluster. Must be sector-aligned. Should only be
  /// modified by \ref read_cluster_to_data_cache().
  const nonstd::unique_ptr<std::byte> data_cache;

  /// Cluster index of cluster stored in \ref data_cache.
  uint32_t data_cache_cluster = -1;
};

} // namespace fs::fat32
