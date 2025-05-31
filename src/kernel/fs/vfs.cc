#include "fs/vfs.h"
#include "fs/drivers/fat32.h"
#include "nonstd/allocator.h"
#include "nonstd/libc.h"
#include "nonstd/node_hash_map.h"
#include "nonstd/string_view.h"
#include "util/assert.h"
#include "util/pathutil.h"
#include <tuple>

namespace {
// nonstd::node_hash_map<nonstd::string_view, fs::Dentry *> path_lookup;
fs::Dentry *root = nullptr;
} // namespace

namespace fs {

namespace {

/// Used for transparent lookups without creating a nonstd::string.
/// Hash specialization defined at the end of this file.
struct DcacheTransparentKey {
  Dentry *parent;
  nonstd::string_view component;

  auto operator<=>(const DcacheTransparentKey &) const = default;
};

/// The key storage type. Can be looked up transparently via a
/// DcacheTransparentKey.
struct DcacheKey {
  Dentry *parent;
  nonstd::string component;

  auto operator<=>(const DcacheKey &) const = default;
  operator DcacheTransparentKey() const { return {parent, component}; }
};

/// Hashtable containing all dcache entries that exist in the
/// filesystem. Dentries can exist and not be in \ref dcache_lookup
/// for two reasons:
///
/// 1. It can be a negative dentry (not implemented yet).
/// 2. It can be removed from the filesystem but a \ref File still
///    has a reference to it.
///
/// This will be initialized in \ref fs::init().
std::optional<nonstd::node_hash_map<DcacheKey, Dentry *>> dcache_lookup;

/// The main pathname lookup logic. Start from the filesystem root,
/// and for each component:
/// 1. If it is . or .., handle appropriately.
/// 2. Otherwise, attempt to look up the name in the dcache
///    hashtable.
/// 3. If not found, delegate to the filesystem's \a lookup()
///    method, which will return an inode. We'll create a dentry from
///    this inode.
/// 4. If not found, return nullptr and set res to
///    Result::FileNotFound. Otherwise continue to the next component
///    (if any).
Dentry *pathname_lookup(nonstd::string_view path, Result &res) {
  Dentry *it = root;
  while (!path.empty()) {
    nonstd::string_view component;
    std::tie(component, path) = util::path::left_partition_path(path);

    if (component.empty() || component == ".") {
      continue;
    }
    if (component == "..") {
      if (it->parent != nullptr) {
        it = it->parent;
      }
      continue;
    }

    auto lookup_it = dcache_lookup->find(DcacheTransparentKey{it, component});
    if (lookup_it != dcache_lookup->end()) {
      // Dentry exists in the dcache (via hashtable lookup).
      it = lookup_it->second;
    } else {
      // Dentry doesn't exist in the dcache, we have to delegate to
      // the filesystem's \a lookup().
      auto *inode = it->inode.lookup(component, res);
      ASSERT((inode == nullptr) ^ (res == Result::Ok));
      if (inode == nullptr) {
        return nullptr;
      }
      it = new Dentry{it, *inode, component};
    }
  }

  return it;
}

} // namespace

/// See note about dynamic allocation for Inodes. The same applies
/// here.
Dentry::Dentry(Dentry *_parent, Inode &_inode, nonstd::string_view _component)
    : inode{_inode}, parent{_parent}, component{_component} {
  if (likely(parent)) {
    parent->inc_rc();
    parent->children.push_back(*this);

    const auto [_, inserted] =
        dcache_lookup->try_emplace(DcacheKey{parent, component}, this);
    ASSERT(inserted);
    in_hashtable = true;
  }
  inode.inc_rc();
}

Dentry::~Dentry() {
  ASSERT(children.empty());

  inode.dec_rc();
  if (likely(parent)) {
    DentrySiblingList::erase();
    parent->dec_rc();

    if (in_hashtable) {
      const auto it =
          dcache_lookup->find(DcacheTransparentKey{parent, component});
      ASSERT(it != dcache_lookup->end() && it->second == this);
      dcache_lookup->erase(it);
      in_hashtable = false;
    }
  }
}

FileDescriptor Process::get_next_fd() {
  for (FileDescriptor fd = 0; fd < fds.size(); ++fd) {
    if (fds[fd] == std::nullopt) {
      return fd;
    }
  }

  // No unused fds, allocate a new one.
  fds.emplace_back();
  return fds.size() - 1;
}

FileDescriptor Process::open(nonstd::string_view path, Result &res) {
  Dentry *dentry = pathname_lookup(path, res);
  if (dentry == nullptr) {
    return InvalidFD;
  }

  const FileDescriptor fd = get_next_fd();
  fds[fd].emplace(*dentry, fd, res);
  return fd;
}

void Process::close(FileDescriptor fd, Result &res) {
  if (fd >= fds.size() || fds[fd] == std::nullopt) {
    res = Result::BadFD;
    return;
  }

  /// Closing is done in the \ref File destructor.
  fds[fd].reset();
}

void Process::creat(nonstd::string_view path, Result &res) {
  Dentry *dentry = pathname_lookup(path, res);

  // If file already exists, truncate it (similar to creat(2)) and
  // return early.
  if (dentry != nullptr) {
    if (dentry->inode.is_directory) {
      res = Result::IsDirectory;
      return;
    }

    truncate(path, 0, res);
    return;
  }

  // File doesn't exist, let's try creating it. The parent should
  // already have been added to the hashtable by the last call to
  // add_dentry_to_cache.
  const auto [dirname, basename] = util::path::partition_path(path);
  Dentry *parent = pathname_lookup(dirname, res);
  if (parent == nullptr) {
    res = Result::FileNotFound;
    return;
  } else if (!parent->inode.is_directory) {
    res = Result::IsFile;
    return;
  }

  res = parent->inode.creat(basename);
}

ssize_t Process::read(FileDescriptor fd, void *buf, size_t count, Result &res) {
  if (fd >= fds.size() || fds[fd] == std::nullopt) {
    res = Result::BadFD;
    return -1;
  }

  const ssize_t rval =
      fds[fd]->dentry->inode.read(buf, fds[fd]->offset, count, res);
  ASSERT((rval < 0) ^ (res == Result::Ok));
  if (rval > 0) {
    fds[fd]->offset += rval;
  }
  return rval;
}

// TODO: implement these
void Process::truncate(nonstd::string_view path, uint64_t len, Result &res) {}
void Process::mkdir(nonstd::string_view path, Result &res) {}
void Process::rmdir(nonstd::string_view path, Result &res) {}
void Process::link(nonstd::string_view target, nonstd::string_view link,
                   Result &res) {}
void Process::unlink(nonstd::string_view link, Result &res) {}

void init(Filesystem &root_fs) {
  root = root_fs.get_root_dentry();
  dcache_lookup.emplace();
  ASSERT(root != nullptr);
  ASSERT(dcache_lookup.has_value());
}

} // namespace fs

namespace nonstd {
template <> struct hash<fs::DcacheTransparentKey> {
  size_t operator()(const fs::DcacheTransparentKey &key) {
    return hash_combine(key.parent, key.component);
  }
};
template <> struct hash<fs::DcacheKey> {
  size_t operator()(const fs::DcacheKey &key) {
    return hash<fs::DcacheTransparentKey>{}(key);
  }
};
} // namespace nonstd
