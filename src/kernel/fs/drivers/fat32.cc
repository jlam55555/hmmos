#include "fat32.h"
#include "drivers/ahci.h"
#include "libc_minimal.h"
#include "nonstd/allocator.h"
#include "nonstd/queue.h"
#include "nonstd/string.h"
#include "perf.h"
#include "util/algorithm.h"
#include "util/assert.h"
#include <functional>

namespace {

/// Returns a unique_ptr to a memory area of size \ref sz.
///
inline nonstd::unique_ptr<std::byte> scoped_buf(size_t sz) {
  return nonstd::unique_ptr(reinterpret_cast<std::byte *>(::operator new(sz)));
}

/// Mask cluster index. The 4 MSB are reserved and should be ignored
/// if set when addressing a cluster.
///
uint32_t mask_cluster(uint32_t cluster) { return cluster & 0x0FFFFFFFU; }

/// Highest 8 cluster indices are reserved for "EOF" cluster marker,
/// something that dates back to the FAT12 days. Conformant FAT32
/// implementations are allowed to use any of these values.
///
bool is_last_cluster_in_file(uint32_t cluster) {
  return mask_cluster(cluster) >= 0x0FFFFFF7U;
}

/// Convert a filename from "regular" to an 8.3 format.
///
/// If the filename or extension are longer than 8 and 3 characters
/// respectively, they will be truncated.
///
/// \return a non-null-terminated 8.3-style filename
std::array<char, 11>
convert_normal_to_8_3_filename(nonstd::string_view filename) {
  // Convert path component to 8.3 format so we can easily compare
  // strings.
  std::array<char, 11> rval = {' ', ' ', ' ', ' ', ' ', ' ',
                               ' ', ' ', ' ', ' ', ' '};
  if (auto pos = filename.find("."); pos == nonstd::string_view::npos) {
    nonstd::memcpy((void *)rval.data(), filename.data(),
                   std::min(filename.length(), (size_t)8));
  } else {
    nonstd::memcpy((void *)rval.data(), filename.data(),
                   std::min(pos, (size_t)8));
    nonstd::memcpy((void *)(rval.data() + 8), filename.data() + pos + 1,
                   std::min(filename.length() - (pos + 1), (size_t)3));
  }
  return rval;
}

/// Convert a filename from 8.3 format to a "regular" one.
///
/// This is slower than the inverse, so you should do that when
/// possible.
///
/// \return a std::array to avoid dynamic allocation but still return
/// a C++ object.  You can use \a rval.data() to get a
/// (null-terminated) const char* out of it.
std::array<char, 13> convert_8_3_to_normal_filename(nonstd::string_view _8_3) {
  std::array<char, 13> rval{};

  assert(_8_3.length() == 11);

  auto filename = _8_3.substr(0, 8);
  auto fileext = _8_3.substr(8);

  auto filename_len = filename.find(' ');
  auto fileext_len = fileext.find(' ');
  if (filename_len == nonstd::string_view::npos) {
    filename_len = 8;
  }
  if (fileext_len == nonstd::string_view::npos) {
    fileext_len = 3;
  }

  nonstd::memcpy(rval.data(), filename.data(), filename_len);
  if (fileext_len) {
    rval[filename_len] = '.';
    nonstd::memcpy(rval.data() + filename_len + 1, fileext.data(), fileext_len);
  }
  return rval;
}

} // namespace

namespace fs::fat32 {

// copied from bootloader
struct VBR {
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
static_assert(sizeof(VBR) == 512);

/// copied from bootloader
struct DirectoryEntry {
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
static_assert(sizeof(DirectoryEntry) == 32);

Inode::Inode(unsigned _id, Filesystem &_fs, uint32_t _start_cluster,
             uint32_t _file_sz_bytes, bool _is_directory,
             std::array<char, 13> _name)
    : fs::Inode{_id, _is_directory}, fs{_fs}, start_cluster{_start_cluster},
      file_sz_bytes{_file_sz_bytes} {
  nonstd::strncpy(name, _name.data(), _name.size());
  const auto [_, inserted] =
      fs.start_cluster_to_inode.try_emplace(start_cluster, this);
  ASSERT(inserted);
}

Inode::~Inode() {
  const auto it = fs.start_cluster_to_inode.find(start_cluster);
  ASSERT(it != fs.start_cluster_to_inode.end() && it->second == this);
  fs.start_cluster_to_inode.erase(it);
}

ssize_t Inode::read(void *buf, size_t offset, size_t count, Result &res) {
  if (is_directory) {
    res = Result::IsDirectory;
    return -1;
  }

  if (offset >= file_sz_bytes) {
    return 0;
  }

  // buf_pos is redundant since it can be computed from offset and
  // file_pos, but it's straightforward.
  uint32_t cur_cluster = start_cluster;
  size_t file_pos = 0;
  size_t buf_pos = 0;
  const size_t file_sz = file_sz_bytes;

  // This is more naturally written recursively, but it is iterative
  // to avoid stack overflows.
  while (1) {
    if (offset >= fs.sectors_per_cluster * 512) {
      // Skip this cluster entirely, no need to read it.
      offset -= fs.sectors_per_cluster * 512;
      file_pos += fs.sectors_per_cluster * 512;
    } else {
      // If the input buffer is aligned we can directly copy to the
      // output buf and avoid the extra memcpy. The extra memcpy
      // keeps things simple though.
      fs.read_cluster_to_data_cache(cur_cluster);

      size_t bytes_to_read_from_cluster =
          std::min(file_sz - file_pos, (size_t)fs.sectors_per_cluster * 512);
      size_t bytes_to_write_to_buf =
          std::min(bytes_to_read_from_cluster - offset, count);
      assert(bytes_to_read_from_cluster > 0);
      assert(bytes_to_write_to_buf > 0);

      nonstd::memcpy((char *)buf + buf_pos, fs.data_cache.get() + offset,
                     bytes_to_write_to_buf);
      file_pos += bytes_to_read_from_cluster;
      buf_pos += bytes_to_write_to_buf;
      offset = 0;
    }

    if (file_pos == file_sz || buf_pos == count) {
      return buf_pos;
    }

    cur_cluster = fs.advance_cluster(cur_cluster);
  }

  __builtin_unreachable();
}

Inode *Inode::lookup(nonstd::string_view _name, Result &res) const {
  // This should never be called on a regular file.
  ASSERT(is_directory);

  const auto path_component_8_3 = convert_normal_to_8_3_filename(_name);

  std::optional<DirectoryEntry> dirent;
  fs.iterate_dir(start_cluster, [&](const auto *it) {
    // Note: VFAT is not supported.
    if (nonstd::string_view{path_component_8_3.data(), 11} ==
        nonstd::string_view{it->short_filename, 11}) {
      dirent.emplace(*it);
      return true;
    }
    return false;
  });

  if (!dirent) {
    res = Result::FileNotFound;
    return nullptr;
  }

  const uint32_t start_cluster =
      /*_start_cluster=*/((uint32_t)dirent->cluster_hi << 2) |
      dirent->cluster_lo;

  // FAT32 doesn't support hardlinks, so we expect to see the child
  // inode in the dcache if it exists. This is a failsafe to prevent
  // us from duplicating inodes.
  if (auto it = fs.start_cluster_to_inode.find(start_cluster);
      unlikely(it != fs.start_cluster_to_inode.end())) {
    // TODO: need some sort of syslog.
    nonstd::printf("Found existing FAT32 inode but it was not in its parent's "
                   "list of children! Is the filesystem corrupted?\r\n");
    return it->second;
  }

  return new Inode{
      fs.next_inode++,
      fs,
      start_cluster,
      dirent->file_sz_bytes,
      bool(dirent->attr.subdir),
      convert_8_3_to_normal_filename(
          nonstd::string_view{dirent->short_filename, 11}),
  };
}

Filesystem::Filesystem(const VBR &vbr, const MBRPartition &part)
    : bytes_per_sector{vbr.ebpb.bytes_per_sector},
      sectors_per_cluster{vbr.ebpb.sectors_per_cluster},
      dir_entries_per_cluster{512 * sectors_per_cluster /
                              sizeof(DirectoryEntry)},
      fat_offset_lba{part.first_sector_lba + vbr.ebpb.reserved_sectors},
      data_region_offset_lba{part.first_sector_lba + vbr.ebpb.reserved_sectors +
                             (vbr.ebpb.fats * vbr.ebpb.sectors_per_fat2)},
      root_dir_start_cluster{vbr.ebpb.root_dir_start_cluster},
      // Really we only need 512-byte aligned, but the current stupid
      // allocator doesn't currently guarantee this. It does guarantee
      // that allocating >= 1PG is page-aligned however. This will be
      // fixed with the slab allocator.
      fat_cache{scoped_buf(PG_SZ)}, //
      data_cache{
          scoped_buf(std::min(PG_SZ, (size_t)sectors_per_cluster * 512))} {
  ASSERT(util::algorithm::aligned_pow2<512>((size_t)fat_cache.get()));
  ASSERT(util::algorithm::aligned_pow2<512>((size_t)data_cache.get()));
}

std::optional<MBRPartition> Filesystem::find_boot_part() {
  // Really we only need 512-byte aligned, but the current stupid
  // allocator doesn't currently guarantee this. It does guarantee
  // that allocating >= 1PG is page-aligned however. This will be
  // fixed with the slab allocator.
  auto buf = scoped_buf(PG_SZ);
  assert(drivers::ahci::read_blocking(0, 0, 0, 1,
                                      reinterpret_cast<uint16_t *>(buf.get())));
  // I'm lazy -- hardcode the start of the partition table in the MBR here.
  const auto *partitions = reinterpret_cast<MBRPartition *>(buf.get() + 0x01BE);
  for (int i = 0; i < 4; ++i) {
    if (partitions[i].partition_type == 0x0C) {
      return partitions[i];
    }
  }
  return {};
}

Filesystem Filesystem::from_partition(MBRPartition &boot_part) {
  // Really we only need 512-byte aligned, but the current stupid
  // allocator doesn't currently guarantee this. It does guarantee
  // that allocating >= 1PG is page-aligned however. This will be
  // fixed with the slab allocator.
  auto buf = scoped_buf(PG_SZ);
  assert(drivers::ahci::read_blocking(0, boot_part.first_sector_lba, 0, 1,
                                      reinterpret_cast<uint16_t *>(buf.get())));
  return Filesystem{reinterpret_cast<VBR &>(*buf), boot_part};
}

uint32_t Filesystem::get_fat_sector_for_cluster(uint32_t cluster) {
  return fat_offset_lba + mask_cluster(cluster) / fat_entries_per_sector;
}

void Filesystem::iterate_dir(uint32_t dir_cluster,
                             std::function<bool(const DirectoryEntry *)> cb) {
  // This is more naturally written recursively, but it is iterative
  // to avoid stack overflows.
  while (1) {
    read_cluster_to_data_cache(dir_cluster);

    const auto *entries = reinterpret_cast<DirectoryEntry *>(data_cache.get());
    for (auto *it = entries; it < entries + dir_entries_per_cluster; ++it) {
      switch (it->short_filename[0]) {
      case '\0':
        // No more directory entries.
        return;
      case (char)0xE5:
        // Deleted file.
        continue;
      }

      // Skip VFAT entries.
      if (bw::id(it->attr) == 0x0F) {
        continue;
      }

      if (cb(it)) {
        return;
      }
    }

    dir_cluster = advance_cluster(dir_cluster);
  }

  __builtin_unreachable();
}

uint32_t Filesystem::advance_cluster(uint32_t cur_cluster) {
  auto fat_it = get_fat_sector_for_cluster(cur_cluster);
  if (fat_it != fat_cache_lba) {
    assert(drivers::ahci::read_blocking(
        0, fat_it, 0, 1, reinterpret_cast<uint16_t *>(fat_cache.get())));
    fat_cache_lba = fat_it;
  }

  uint32_t next_cluster = mask_cluster(reinterpret_cast<uint32_t *>(
      fat_cache.get())[cur_cluster % fat_entries_per_sector]);
  // Assumed in the preconditions of this function.
  assert(!is_last_cluster_in_file(next_cluster));
  return next_cluster;
}

void Filesystem::read_cluster_to_data_cache(uint32_t cluster) {
  if (cluster == data_cache_cluster) {
    // Already cached; nothing to do here.
    return;
  }

  // -2 because cluster index 2 is the first cluster in the data region.
  assert(drivers::ahci::read_blocking(
      0, data_region_offset_lba + (cluster - 2) * sectors_per_cluster, 0,
      sectors_per_cluster, reinterpret_cast<uint16_t *>(data_cache.get())));
  data_cache_cluster = cluster;
}

} // namespace fs::fat32
