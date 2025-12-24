#pragma once

/// \file vfs.h
/// \brief Virtual filesystem
///
/// This is the implementation of the "everything is a file"
/// interface. This comprises the following components:
///
/// - Inode ("index node"): An in-memory representation of a
///   filesystem object. Each filesystem type (e.g., FAT, ext4,
///   in-memory filesystems like tmpfs) have to implement their own
///   Inode type with its own set of file operations. These are
///   reference counted and automatically destroyed.
///
/// - Dentry: ("directory entry"): A handle to an inode, along with
///   path information. These are the VFS's filesystem-independent
///   in-memory representation of filesystem objects, and play an
///   important part in pathname resolution. Collectively, dentries
///   form the "dcache" (dentry cache), which allow us to avoid
///   expensive filesystem reads on every filesystem lookup. Path
///   lookups first perform a hashtable lookup of the dcache before
///   falling back to the filesystem's \a lookup() method.
///
/// - File: The representation of a file opened by a process. This
///   wraps a dentry with seek position. Any number of Files can refer
///   to the same Dentry (and will prevent the file from being deleted
///   while it is open).
///
/// - Filesystem: In this context, the filesystem is the class that
///   maintains superblock information and imbues inodes with its set
///   of file operations. Of course, we'll need a different filesystem
///   implementation for each filesystem protocol (e.g., FAT, ext4,
///   etc.). The VFS must register a root filesystem and can mount
///   additional filesystems.
///
/// - Superblock: Colloquially, this refers to a single instance of a
///   filesystem (corresponding to a single disk partition, a single
///   mount point, and represented via a single filesystem
///   object). The more traditional definition refers to the on-disk
///   metadata associated with one such filesystem instance.
///
/// TODO: make this thread safe.

#include "fs/result.h"
#include "nonstd/allocator.h"
#include "nonstd/node_hash_map.h"
#include "nonstd/string.h"
#include "nonstd/string_view.h"
#include "nonstd/vector.h"
#include "util/intrusive_list.h"
#include "util/objutil.h"
#include <optional>

namespace fs {

class Inode {
public:
  /// Inodes (and dentries) are shared objects and must be dynamically
  /// allocated, because they will be `delete`-d when their refcount
  /// drops to zero. We don't want the destructor logic to be called
  /// when a stack-allocated object goes out of scope. For special
  /// unlinkable inodes like the root inode, an indestructible object
  /// can be used instead of heap allocation.
  ///
  /// TODO: replace dynamic allocation with slab allocator
  Inode(unsigned _id, bool _is_directory)
      : id{_id}, is_directory{_is_directory} {}
  virtual ~Inode() {
    // TODO: if this file is unlinked when deleted, actually clear its
    // contents.
  }

  NON_MOVABLE(Inode);

  /// Inodes should only be referenced via Dentry.
  /// TODO: add AccessKey<Dentry>
  void inc_rc() { ++rc; }

  /// Returns whether the refcount was decremented to zero, i.e. the
  /// dentry is able to be freed. In practice, the return value
  /// shouldn't be used by the caller since it doesn't care about this
  /// object anymore (\see Dentry dtor).
  bool dec_rc() {
    ASSERT(rc != 0);
    if (--rc == 0) {
      // This assumes that the Inode is dynamically allocated.  The
      // root node may not be dynamically allocated, but it should
      // also never have a refcount of zero (it cannot be removed from
      // the hashtable, which should always maintain a reference to
      // it).
      delete this;
      return true;
    }
    return false;
  }

  uint32_t id;
  unsigned rc = 0;
  bool is_directory;

  // File-only operations.
  virtual ssize_t read(void *buf, size_t offset, size_t count, Result &res) = 0;
  virtual Result write(void *buf, size_t offset, size_t count) = 0;
  virtual Result truncate(size_t len) = 0;
  virtual Result mmap(void *addr, size_t offset, size_t count) = 0;
  virtual Result flush() = 0;

  // Directory-only operations.
  virtual Result creat(nonstd::string_view name) = 0;
  virtual Result mkdir(nonstd::string_view name) = 0;
  virtual Result rmdir(nonstd::string_view name) = 0;
  virtual Result link(Inode &new_parent, nonstd::string_view name) = 0;
  virtual Result unlink() = 0;
  // TODO: readdir -- what should the interface look like

  // Directory-only operation: return child inode object if it exists
  // in the filesystem, creating it if necessary. Do not increase the
  // child's refcount. Do not actually modify the filesystem on
  // disk. Returns nullptr if the child doesn't exist.
  virtual Inode *lookup(nonstd::string_view name, Result &res) const = 0;
};

class Dentry;
using DentrySiblingList = util::IntrusiveListHead<Dentry>;
class Dentry : public DentrySiblingList {
public:
  Dentry(Dentry *_parent, Inode &_inode, nonstd::string_view _component);
  ~Dentry();

  NON_MOVABLE(Dentry);

  /// Inodes should only be referenced via File, Dentry, and
  /// PathHastable.
  /// TODO: add MultiAccessKey<File, Dentry, PathHastable>
  void inc_rc() { ++rc; }

  /// Returns whether the refcount was decremented to zero, i.e. the
  /// dentry is able to be freed. In practice, the return value
  /// shouldn't be used by the caller since it doesn't care about this
  /// object anymore.
  bool dec_rc() {
    ASSERT(rc != 0);
    if (--rc == 0) {
      // This assumes that the Dentry is dynamically allocated.  The
      // root node may not be dynamically allocated, but it should
      // also never have a refcount of zero (it cannot be removed from
      // the hashtable, which should always maintain a reference to
      // it).
      //
      // TODO: only delete is memory is low, otherwise wait for
      // garbage collector.
      delete this;
      return true;
    }
    return false;
  }

  Inode &inode;
  Dentry *parent;

  /// Not used by the pathname lookup logic. It can be used to dump
  /// the dcache contents.
  DentrySiblingList children;

  nonstd::string component;

  bool in_hashtable = false;
  bool to_unlink = false;
  unsigned rc = 0;
};

using FileDescriptor = unsigned;
constexpr FileDescriptor InvalidFD = -1;

/// Representation of an open file for a process.
///
class File {
public:
  // Unlike inodes/dentries, Files are owned by a single process.
  File(Dentry &_dentry, FileDescriptor fd) : dentry{&_dentry} {
    dentry->inc_rc();
  }
  ~File() {
    if (dentry) {
      // \a dentry can be nullptr if this File was moved away from
      // (when resizing the FD vector).
      dentry->dec_rc();
    }
  }

  File(File &&f) { *this = std::move(f); }
  File &operator=(File &&f) {
    std::swap(dentry, f.dentry);
    std::swap(offset, f.offset);
    std::swap(fd, f.fd);
    return *this;
  }

  NON_COPYABLE(File);

  Dentry *dentry = nullptr;
  uint64_t offset = 0;
  FileDescriptor fd = 0;
};

class Filesystem {
public:
  virtual Dentry *get_root_dentry() = 0;
};

void init(Filesystem &root_fs);

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
Dentry *pathname_lookup(nonstd::string_view path, Result &res);

} // namespace fs
