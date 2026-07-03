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
/// A process owns the following resources (which need to be cleaned
/// up on exec/fork/exit):
///
/// 1. Kernel thread
/// 2. Memory mappings / page table
/// 3. Open file descriptors
/// 4. The \ref Process object itself
///

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
    uint8_t rsv0 : 6 = 0;
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

  // TODO: rmap mechanism to allow page cache eviction for shared or
  // file-backed files. To implement this, this will exist in a linked
  // list of VMAs for this file, and it should contain a reference to
  // the page table. On page cache eviction, we'll walk these VMAs and
  // unmap all page table entries that map this page. Currently, pages
  // can never be evicted from the page cache while mapped.
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

  /// The kernel thread is destroyed by exit(), which is called after
  /// this destructor, and after this field's destructors. We can't
  /// call it at the end of this destructor because we lose execution
  /// control before calling the field constructors. This has the side
  /// effect that the thread object can be shared between the original
  /// and replaced program.
  ///
  /// There's nothing to do in this destructor itself -- teardown is
  /// handled by field destructors, in reverse order of construction:
  ///
  /// 1. Memory mappings are unmapped by destructing \ref vmas.
  /// 2. The custom page tables are freed by destructing \ref
  ///    page_directory.
  /// 3. File descriptors are closed by destructing \ref fds.
  ~Process() = default;

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

  /// mmap(): Creates a virtual memory mapping for this
  /// process. Mappings must have a page-aligned starting address and
  /// length. There are two primary orthogonal characteristics of
  /// mappings: (1) shared vs. private; and (2) anonymous
  /// vs. file-backed. Modifications to writable shared mappings are
  /// visible to other processes sharing this mapping (via
  /// fork). File-backed mappings have their contents populated to a
  /// file, and modifications to writable mappings are written back to
  /// the file, whereas anonymous pages are not backed by the
  /// filesystem.
  ///
  /// Mapping pages into the page table happens lazily. On mmap(), a
  /// VMA (virtual memory area) struct describing the new mapping is
  /// created. Accessing this newly mapped memory will generate a page
  /// fault, which will scan the process for a matching VMA, at which
  /// point mapping the page into the page table.
  ///
  /// The \a unmap() operation iterates the page table and unmaps each
  /// mapped (faulted-in) page individually. Unmapping a page involves
  /// removing from the page table, decreasing its refcount in the
  /// page frame table, and clearing from the TLB. Note that it is
  /// also possible for a (shared mapping) page to be evicted from the
  /// page cache without an unmap() operation, which forces it to be
  /// unmapped from all processes that it is mapped into. Like
  /// unmap(), this causes the page to no longer be in the process's
  /// page tables, but since the VMA still exists, it will be
  /// reinstated on a future page fault.
  ///
  /// Shared mappings are always backed by the page cache. Shared
  /// anonymous mappings use a temporary shmem file. Private anonymous
  /// pages do not use the page cache. Private file-backed mappings
  /// initially are page-cached based, but behave identically to
  /// private anonymous pages after CoW (see below).
  ///
  /// CoW (Copy on Write) is an optimization to avoid unnecessary
  /// allocations on writable private mapped pages. The behavior
  /// differs between anonymous and file-backed mappings. Assume that
  /// CR0.WP (write protection) is enabled after mmap/fork, so that
  /// kernel writes to read-only pages will trigger a page fault.
  ///
  /// - Anonymous: Writable private anonymous mapped pages are marked
  ///   read-only in both the parent and child's page tables during
  ///   fork(). On a page fault, the kernel notices that the VMA
  ///   references a writable segment, which triggers CoW
  ///   semantics. If this is not the only reference to this CoW
  ///   mapping, a new page is allocated and the page table entry is
  ///   remapped; otherwise the current page is marked as writable.
  ///
  /// - File-backed: On mmap(), writable private file-backed mappings
  ///   are marked read-only. On a page fault, the kernel notices that
  ///   the VMA references a writable segment, which triggers CoW
  ///   semantics. A new page is allocated and the page table entry is
  ///   remapped. This new page behaves like a writable private
  ///   anonymous mapped page (undergoing CoW on fork()).
  ///
  /// I/O to shared mappings is coherent with file I/O via
  /// non-O_DIRECT read/write syscalls. However, coherency with
  /// O_DIRECT read/writes is not guaranteed. Similarly, writing to a
  /// file that is mapped via a private file-backed mapping via
  /// another process is undefined behavior due to CoW.
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

  /// Virtual address space. This clones the kernel page directory,
  /// then deletes the clone on process destruction.
  struct ScopedPageDirectory {
    ScopedPageDirectory();
    ~ScopedPageDirectory();
    operator arch::page_table::PageDirectoryEntry *() const { return data; }

  private:
    arch::page_table::PageDirectoryEntry *data;
  } page_directory;

  /// Virtual memory areas/mappings, sorted by address.
  nonstd::list<VirtualMemoryArea> vmas;
};

} // namespace proc
