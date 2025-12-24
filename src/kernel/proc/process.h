#pragma once

/// \file process.h
/// \brief Process abstraction (a.k.a. userspace thread).
///
/// Syscalls are implemented as methods of \ref Process.
///
/// The process of creating a process (via fork/exec):
/// 1. Map memory from executable.
/// 2. Setup standard FDs.
/// 3. Setup kernel thread (which simply jumps to userspace start
///    label).
/// 4. Add thread to scheduler.
///
/// Syscalls are implemented as methods of \ref Process.

#include "fs/result.h"
#include "fs/vfs.h"
#include "page_table.h"
#include "sched/kthread.h"

namespace proc {

/// Representation of a memory mapping, a.k.a. "virtual memory area"
/// or VMA. Akin to Linux's \a vm_area_struct.
///
/// Like \ref fs::File, this is owned by a single process and
/// maintains a reference count on a dentry. A \ref VirtualMemoryArea can only
/// be created from an open \a fs::File, but does not require that the
/// \a fs::File be kept open.
class VirtualMemoryArea {
public:
  struct Access {
    bool executable : 1 = false;
    bool writable : 1 = false;
    bool readable : 1 = false;
    uint8_t rsv0 : 5 = 0;
  };

  struct Flags {
    bool map_anon : 1 = false;
    bool map_private : 1 = false;
    bool map_shared : 1 = false;
    uint8_t rsv0 : 5 = 0;
  };

  VirtualMemoryArea(size_t addr, size_t len, Access prot, Flags flags,
                    fs::Dentry *dentry, size_t offset, fs::Result &res);
  ~VirtualMemoryArea();

  NON_COPYABLE(VirtualMemoryArea);

  VirtualMemoryArea(VirtualMemoryArea &&vma) { *this = std::move(vma); }
  VirtualMemoryArea &operator=(VirtualMemoryArea &&vma) {
    std::swap(addr, vma.addr);
    std::swap(len, vma.len);
    std::swap(prot, vma.prot);
    std::swap(flags, vma.flags);
    std::swap(dentry, vma.dentry);
    std::swap(offset, vma.offset);
    return *this;
  }

  size_t addr;
  size_t len;
  Access prot;
  Flags flags;

  // File-backed memory map only. We keep a reference to the \a
  // fs::Dentry object rather than the \a fs::Inode so that we have
  // path information. In Linux, the \a vm_area_struct holds a
  // reference to the \a fs::File object, but in HmmOS the \a fs::File
  // object isn't reference counted.
  fs::Dentry *dentry = nullptr;
  size_t offset = 0;
};

/// Options for lseek(2)'s \a whence parameter.
enum class Seek {
  Set, /// Offset is absolute
  Cur, /// Offset is relative to current seek position
  End, /// Offset is relative to end of file
};

class Process {
public:
  Process(sched::Scheduler &sched, nonstd::string_view bin_path,
          fs::Result &res);
  ~Process();

  fs::FileDescriptor open(nonstd::string_view path, fs::Result &res);
  void close(fs::FileDescriptor fd, fs::Result &res);
  void creat(nonstd::string_view path, fs::Result &res);
  void truncate(nonstd::string_view path, uint64_t len, fs::Result &res);
  void mkdir(nonstd::string_view path, fs::Result &res);
  void rmdir(nonstd::string_view path, fs::Result &res);
  void link(nonstd::string_view target, nonstd::string_view link,
            fs::Result &res);
  void unlink(nonstd::string_view link, fs::Result &res);
  void lseek(fs::FileDescriptor fd, ssize_t offset, Seek whence,
             fs::Result &res);
  void *mmap(size_t addr, size_t length, VirtualMemoryArea::Access prot,
             VirtualMemoryArea::Flags flags, fs::FileDescriptor fd,
             size_t offset, fs::Result &res);
  ssize_t read(fs::FileDescriptor fd, void *buf, size_t count, fs::Result &res);
  void exit(int status);

  /// Used by page fault handler.
  const nonstd::list<VirtualMemoryArea> &get_vmas() const { return vmas; }

  /// Used by scheduler.
  void enter_virtual_address_space() const;

private:
  /// Returns the smallest available file descriptor.
  fs::FileDescriptor get_next_fd();

  /// Sets up memory mappings for text and data regions from the ELF
  /// file.
  void map_elf_segments(nonstd::string_view bin_path, fs::Result &res);

  /// Enters userspace. This is the code that gets scheduled for this
  /// process.
  __attribute__((noreturn)) void jump_to_userspace();

  sched::Scheduler &sched;

  /// Used by \ref jump_to_userspace.
  void *eip3 = nullptr;
  void *esp3 = nullptr;

  sched::ThreadID tid = sched::InvalidTID;

  /// nullopt if file is closed.
  nonstd::vector<std::optional<fs::File>> fds;

  /// Virtual memory areas/mappings, sorted by address.
  nonstd::list<VirtualMemoryArea> vmas;

  /// Virtual address space.
  arch::page_table::PageDirectoryEntry *page_directory;
};

} // namespace proc
