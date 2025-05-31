#pragma once

/// \file fat32.h
/// \brief FAT32 filesystem driver
///
/// TODO: make this thread-safe

#include "drivers/ahci.h"
#include "fs/vfs.h"
#include "libc_minimal.h"
#include "memdefs.h"
#include "nonstd/allocator.h"
#include "nonstd/libc.h"
#include "nonstd/memory.h"
#include "nonstd/node_hash_map.h"
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

/// FAT directory entry struct. Not exposed externally.
struct DirectoryEntry;

class Filesystem;
class Inode final : public fs::Inode {
public:
  Inode(unsigned _id, Filesystem &_fs, uint32_t _start_cluster,
        uint32_t _file_sz_bytes, bool _is_directory,
        std::array<char, 13> _name);
  ~Inode();

  ssize_t read(void *buf, size_t offset, size_t count, Result &res) final;

  // TODO
  Result write(void *buf, size_t offset, size_t count) final {
    return Result::Unsupported;
  }
  Result truncate(size_t len) final { return Result::Unsupported; }
  Result mmap(void *addr, size_t offset, size_t count) final {
    return Result::Unsupported;
  }
  Result flush() final { return Result::Unsupported; }

  // Directory entries.
  virtual Result creat(nonstd::string_view name) final {
    return Result::Unsupported;
  }
  virtual Result mkdir(nonstd::string_view name) final {
    return Result::Unsupported;
  }
  virtual Result rmdir(nonstd::string_view name) final {
    return Result::Unsupported;
  }
  virtual Result link(fs::Inode &new_parent, nonstd::string_view name) final {
    return Result::Unsupported;
  }
  virtual Result unlink() final { return Result::Unsupported; }
  virtual Inode *lookup(nonstd::string_view name, Result &res) const final;

private:
  Filesystem &fs;
  uint32_t start_cluster;
  uint32_t file_sz_bytes;
  /// Null-terminated "normal" filename
  char name[13];
};

/// Interface for interacting with FAT32 filesystem.
///
class Filesystem final : public fs::Filesystem {
  friend class Inode;

public:
  /// Returns the boot FAT32 partition.
  ///
  /// This reads the MBR, duplicating some work from the bootloader.
  ///
  static std::optional<MBRPartition> find_boot_part();

  /// Initialize a FAT32 filesystem object for the given partition.
  ///
  static Filesystem from_partition(MBRPartition &boot_part);

  Dentry *get_root_dentry() final {
    static auto *root_inode = new Inode{
        next_inode++,           //
        *this,                  //
        root_dir_start_cluster, //
        /*_file_sz_bytes=*/0,   //
        /*_is_directory=*/true, //
        std::array<char, 13>{}, //
    };
    static auto *root_dentry =
        new Dentry{/*parent=*/nullptr, *root_inode, "<root>"};

    // This dentry/inode should never be recycled. Without the
    // following, this dentry would have a refcount of 0, which means
    // it might get cleaned up the next time someone dec_rc()'s it to
    // 0, and bad things (TM) will happen.
    //
    // TODO: Is it important to clean this up on shutdown?
    root_dentry->inc_rc();

    return root_dentry;
  }

private:
  static constexpr unsigned fat_entries_per_sector = 512 / sizeof(uint32_t);

  Filesystem(const VBR &vbr, const MBRPartition &part);

  /// Helper function for iterating directories on disk.
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

  /// Next inode number.
  uint32_t next_inode = 0;

  /// Mapping of start_cluster -> inode. All inodes in this superblock
  /// must live in this hashmap. See usage in \ref Inode::lookup().
  ///
  /// TODO: This will also be used by readdir(), once we implement
  /// that.
  ///
  /// N.B. at any given point of time, start cluster uniquely
  /// identifies a file/directory in the superblock.
  nonstd::node_hash_map<uint32_t, Inode *> start_cluster_to_inode;
};

} // namespace fs::fat32
