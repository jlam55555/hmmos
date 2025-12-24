#include "proc/process.h"
#include "fs/result.h"
#include "fs/vfs.h"
#include "gdt.h"
#include "libc_minimal.h"
#include "memdefs.h"
#include "mm/virt.h"
#include "nonstd/memory.h"
#include "page_table.h"
#include "proc/elf.h"
#include "sched/kthread.h"
#include "stack.h"
#include "util/algorithm.h"
#include "util/pathutil.h"

#define CHECK_RES                                                              \
  if (res != fs::Result::Ok) {                                                 \
    return;                                                                    \
  }

namespace proc {

VirtualMemoryArea::VirtualMemoryArea(size_t _addr, size_t _len, Access _prot,
                                     Flags _flags, fs::Dentry *_dentry,
                                     size_t _offset, fs::Result &res)
    : addr{_addr}, len{_len}, prot{_prot}, flags{_flags}, dentry{_dentry},
      offset{_offset} {

  // Exactly one of MAP_PRIVATE and MAP_SHARED must be specified.
  if (!(flags.map_private ^ flags.map_shared)) {
    res = fs::Result::InvalidArgs;
    return;
  }

  // A mapping is file-backed iff MAP_ANON is specified.
  // (This is a little redundant, but let's be extra safe.)
  if ((dentry == nullptr) != flags.map_anon) {
    res = fs::Result::InvalidArgs;
    return;
  }

  // Address and file offset must be page-aligned. (Length notably
  // isn't required to be.)
  if (!PG_ALIGNED(addr) || !PG_ALIGNED(offset) || len == 0) {
    res = fs::Result::InvalidArgs;
    return;
  }

  if (dentry != nullptr) {
    dentry->inc_rc();
  }

  // We use lazy paging, so there's nothing more to do at this moment.
}

VirtualMemoryArea::~VirtualMemoryArea() {
  if (dentry != nullptr) {
    dentry->dec_rc();
  }

// NOCOMMIT TODO: walk the page table, unmap each page. If the
// mapped page refcount drops to zero, also free the page.
#if 0
  ASSERT(false);
#endif
}

namespace {

class ScopedEnterProcContext {
public:
  ScopedEnterProcContext(Process &proc, sched::Scheduler &_sched,
                         arch::page_table::PageDirectoryEntry *page_directory)
      : sched{_sched}, prev_pd{arch::page_table::get_page_directory()} {
    sched.override_curr_proc(&proc);
    arch::page_table::set_page_directory(page_directory);
  }
  ~ScopedEnterProcContext() {
    arch::page_table::set_page_directory(prev_pd);
    sched.override_curr_proc(nullptr);
  }

private:
  sched::Scheduler &sched;
  arch::page_table::PageDirectoryEntry *prev_pd;
};

} // namespace

Process::Process(sched::Scheduler &_sched, nonstd::string_view bin_path,
                 fs::Result &res)
    : sched{_sched} {

  // This is the page table created by exec(), which only copies the
  // kernel mappings.
  //
  // TODO: more complicated cloning methods for fork(), clone() etc.
  page_directory = arch::page_table::clone_kernel_page_directory(
      arch::page_table::get_page_directory());

  {
    // We have to temporarily enter the context of the new process,
    // because the process of loading the ELF image may involve
    // writing to the userspace-mapped pages of the new process.
    ScopedEnterProcContext guard{*this, sched, page_directory};

    map_elf_segments(bin_path, res);
    CHECK_RES;
  }

  // Map stack region at the top of the virtual address range.  Use
  // 8KB by default. (We should probably use larger, Linux uses 8MB,
  // this should make it easier to accidentally trigger a stack
  // overflow.)
  const size_t stack_size = 8 * KB;
  const size_t stack_top = mem::virt::hhdm_start;
  mmap(stack_top - stack_size, stack_size,
       VirtualMemoryArea::Access{
           .executable = false, .writable = true, .readable = true},
       VirtualMemoryArea::Flags{.map_anon = true, .map_private = true},
       fs::InvalidFD, 0, res);
  CHECK_RES;
  esp3 = (void *)stack_top;

  // TODO: open stdin/stdout/stderr

  tid = sched.new_thread(
      this,
      [](void *p) { reinterpret_cast<Process *>(p)->jump_to_userspace(); },
      this);
}

Process::~Process() {
  // NOCOMMIT TODO: cleanup page tables
  //
  // VM mappings will automatically be cleaned up when the VMA objects
  // get destructed
}

fs::FileDescriptor Process::get_next_fd() {
  for (fs::FileDescriptor fd = 0; fd < fds.size(); ++fd) {
    if (fds[fd] == std::nullopt) {
      return fd;
    }
  }

  // No unused fds, allocate a new one.
  fds.emplace_back();
  return fds.size() - 1;
}

fs::FileDescriptor Process::open(nonstd::string_view path, fs::Result &res) {
  fs::Dentry *dentry = pathname_lookup(path, res);
  if (dentry == nullptr) {
    return fs::InvalidFD;
  }

  const fs::FileDescriptor fd = get_next_fd();
  fds[fd].emplace(*dentry, fd);
  return fd;
}

void Process::close(fs::FileDescriptor fd, fs::Result &res) {
  if (fd >= fds.size() || fds[fd] == std::nullopt) {
    res = fs::Result::BadFD;
    return;
  }

  /// Closing is done in the \ref File destructor.
  fds[fd].reset();
}

void Process::creat(nonstd::string_view path, fs::Result &res) {
  fs::Dentry *dentry = pathname_lookup(path, res);

  // If file already exists, truncate it (similar to creat(2)) and
  // return early.
  if (dentry != nullptr) {
    if (dentry->inode.is_directory) {
      res = fs::Result::IsDirectory;
      return;
    }

    truncate(path, 0, res);
    return;
  }

  // File doesn't exist, let's try creating it. The parent should
  // already have been added to the hashtable by the last call to
  // add_dentry_to_cache.
  const auto [dirname, basename] = util::path::partition_path(path);
  fs::Dentry *parent = pathname_lookup(dirname, res);
  if (parent == nullptr) {
    res = fs::Result::FileNotFound;
    return;
  } else if (!parent->inode.is_directory) {
    res = fs::Result::IsFile;
    return;
  }

  res = parent->inode.creat(basename);
}

void Process::lseek(fs::FileDescriptor fd, ssize_t offset, Seek whence,
                    fs::Result &res) {
  if (fd >= fds.size() || fds[fd] == std::nullopt) {
    res = fs::Result::BadFD;
    return;
  }

  int64_t new_seek = fds[fd]->offset;
  switch (whence) {
  case Seek::Set:
    new_seek = offset;
    break;
  case Seek::Cur:
    new_seek += offset;
    break;
  case Seek::End:
    // TODO: expose file length somewhere
    res = fs::Result::Unsupported;
    return;
  }

  if (new_seek < 0) {
    res = fs::Result::InvalidArgs;
    return;
  }
  fds[fd]->offset = new_seek;
}

void *Process::mmap(size_t addr, size_t length, VirtualMemoryArea::Access prot,
                    VirtualMemoryArea::Flags flags, fs::FileDescriptor fd,
                    size_t offset, fs::Result &res) {
  fs::Dentry *dentry = nullptr;
  if (fd != fs::InvalidFD) {
    if (fd >= fds.size() || fds[fd] == std::nullopt) {
      res = fs::Result::BadFD;
      return nullptr;
    }
    dentry = fds[fd]->dentry;
  }

  auto it = vmas.begin();
  for (; it != vmas.end(); ++it) {
    if (util::algorithm::range_overlaps2(addr, length, it->addr, it->len,
                                         /*inclusive=*/false)) {
      res = fs::Result::MappingExists;
      return nullptr;
    }

    if (it->addr > addr) {
      break;
    }
  }

  // Insert VMA in sorted order.
  auto new_vma =
      vmas.emplace(it, addr, length, prot, flags, dentry, offset, res);
  if (res != fs::Result::Ok) {
    vmas.erase(new_vma);
    return nullptr;
  }
  return reinterpret_cast<void *>(new_vma->addr);
}

ssize_t Process::read(fs::FileDescriptor fd, void *buf, size_t count,
                      fs::Result &res) {
  if (fd >= fds.size() || fds[fd] == std::nullopt) {
    res = fs::Result::BadFD;
    return -1;
  }

  const ssize_t rval =
      fds[fd]->dentry->inode.read(buf, fds[fd]->offset, count, res);
  ASSERT((rval < 0) ^ (res == fs::Result::Ok));
  if (rval > 0) {
    fds[fd]->offset += rval;
  }
  return rval;
}

void Process::exit(int status) {
  ASSERT(tid != sched::InvalidTID);
  sched.destroy_thread(tid);
  delete this;
}

// TODO: implement these
void Process::truncate(nonstd::string_view path, uint64_t len,
                       fs::Result &res) {}
void Process::mkdir(nonstd::string_view path, fs::Result &res) {}
void Process::rmdir(nonstd::string_view path, fs::Result &res) {}
void Process::link(nonstd::string_view target, nonstd::string_view link,
                   fs::Result &res) {}
void Process::unlink(nonstd::string_view link, fs::Result &res) {}

void Process::enter_virtual_address_space() const {
  ASSERT(page_directory != nullptr);
  arch::page_table::set_page_directory(page_directory);
}

void Process::map_elf_segments(nonstd::string_view bin_path, fs::Result &res) {
  auto bin_fd = open(bin_path, res);
  CHECK_RES;

  nonstd::unique_ptr<std::byte> elf_buf{
      reinterpret_cast<std::byte *>(::operator new(PG_SZ))};
  const ssize_t n = read(bin_fd, elf_buf.get(), PG_SZ, res);
  CHECK_RES;

  nonstd::string_view elf_image_buf{(char *)elf_buf.get(), (size_t)n};
  elf::ELFExecutable parsed_elf;
  res = proc::elf::parse_executable(elf_image_buf, parsed_elf);
  CHECK_RES;

  eip3 = (void *)parsed_elf.hdr->entry;

  nonstd::unique_ptr<std::byte> scoped_buf{
      reinterpret_cast<std::byte *>(::operator new(PG_SZ))};

  // Map text and data regions.
  const elf::PHEntry *phentry = parsed_elf.ph_table;
  for (unsigned i = 0; i < parsed_elf.hdr->phnum; ++i, ++phentry) {
    if (phentry->type != elf::PHEntry::Type::Load) {
      continue;
    }

    // The vaddr and offset of a segment don't have to be page-aligned
    // (I noticed this behavior in gcc) but they have to be equal
    // modulo page size so that mmap works correctly.
    if (phentry->vaddr % PG_SZ != phentry->offset % PG_SZ) {
      res = fs::Result::NonExecutable;
      return;
    }

    // Each segment contains:
    //
    // 1. Possibly one non-full file-mapped page (if mapping doesn't
    //    start on page boundary).
    // 2. Possibly some number of full file-mapped pages
    // 3. Possibly one non-full file-mapped page (if mapping doesn't
    //    end on page boundary, and this isn't the same page as (1))
    // 4. Possibly some number of anonymous pages, rounded up to the
    //    next full page.
    //
    // (2) and (4) can be mapped via mmap, (1) and (3) needs to be
    // copied manually into private anonymous mappings as mmap doesn't
    // support partial mappings. Thus we may need to copy up to two
    // pages per segment.
    //
    // An example of a segment that requires all four mappings:
    //
    // LOAD off    0x00002ff4 vaddr 0x0804bff4 paddr 0x0804bff4 align 2**12
    //             filesz 0x0000140c memsz 0x0000340c flags rw-
    // ...
    //
    //   3 .got.plt      0000000c  0804bff4  0804bff4  00002ff4  2**2
    //                   CONTENTS, ALLOC, LOAD, DATA
    //   4 .data         00001400  0804c000  0804c000  00003000  2**5
    //                   CONTENTS, ALLOC, LOAD, DATA
    //   5 .bss          00002000  0804d400  0804d400  00004400  2**5
    //                   ALLOC
    const auto &floor_pg = util::algorithm::floor_pow2<PG_SZ>;
    const auto &ceil_pg = util::algorithm::ceil_pow2<PG_SZ>;
    const VirtualMemoryArea::Access prot{.executable = phentry->executable(),
                                         .writable = phentry->writable(),
                                         .readable = phentry->readable()};
    const VirtualMemoryArea::Flags flags{.map_private = true};
    const VirtualMemoryArea::Flags flags_anon{.map_anon = true,
                                              .map_private = true};
    const auto copy_page = [&](size_t offset) {
      ASSERT(PG_ALIGNED(offset));
      lseek(bin_fd, PG_SZ, Seek::Set, res);
      CHECK_RES;
      const ssize_t n = read(bin_fd, scoped_buf.get(), PG_SZ, res);
      CHECK_RES;
    };

    if (!PG_ALIGNED(phentry->vaddr)) {
      // 1
      mmap(floor_pg(phentry->vaddr), PG_SZ, prot, flags_anon, fs::InvalidFD, 0,
           res);
      CHECK_RES;
      copy_page(floor_pg(phentry->offset));
      CHECK_RES;

      const size_t pg_start = floor_pg(phentry->vaddr);
      const size_t start_off = phentry->vaddr % PG_SZ;
      const size_t end_off =
          std::min(uint64_t(phentry->vaddr + phentry->filesz),
                   ceil_pg(phentry->vaddr)) -
          pg_start;
      nonstd::memset((void *)pg_start, 0, start_off);
      nonstd::memcpy((void *)(pg_start + start_off),
                     scoped_buf.get() + start_off, end_off - start_off);
      nonstd::memset((void *)(pg_start + end_off), 0, PG_SZ - end_off);
    }

    const size_t full_pg_start = ceil_pg(phentry->vaddr);
    const size_t full_pg_end = floor_pg(phentry->vaddr + phentry->filesz);
    if (full_pg_end > full_pg_start) {
      // 2
      mmap(full_pg_start, full_pg_end - full_pg_start, prot, flags, bin_fd,
           phentry->offset + (full_pg_start - phentry->vaddr), res);
      CHECK_RES;
    }

    const size_t filesz_ceil = ceil_pg(phentry->vaddr + phentry->filesz);
    if (filesz_ceil > full_pg_end && full_pg_end >= full_pg_start) {
      // 3
      mmap(full_pg_end, PG_SZ, prot, flags_anon, fs::InvalidFD, 0, res);
      CHECK_RES;
      copy_page(full_pg_end);
      CHECK_RES;

      const size_t pg_start = full_pg_end;
      const size_t end_off = (phentry->vaddr + phentry->filesz) - pg_start;
      nonstd::memcpy((void *)pg_start, scoped_buf.get(), end_off);
      nonstd::memset((void *)(pg_start + end_off), 0, PG_SZ - end_off);
    }

    const size_t memsz_ceil = ceil_pg(phentry->vaddr + phentry->memsz);
    if (memsz_ceil > filesz_ceil) {
      // 4
      mmap(filesz_ceil, memsz_ceil - filesz_ceil, prot, flags_anon,
           fs::InvalidFD, 0, res);
      CHECK_RES;
    }
  }

  close(bin_fd, res);
  CHECK_RES;
}

void Process::jump_to_userspace() {
  // Set esp0 to the top of the current (4KB) stack. This will clobber
  // the small part of the existing stack (which should only be the
  // stack frame of \a jump_to_userspace), but that's okay and
  // expected.
  size_t esp0;
  __asm__("mov %%esp, %0" : : "m"(esp0));
  arch::gdt::set_tss_esp0((void *)util::algorithm::ceil_pow2<PG_SZ>(esp0));

  arch::sched::enter_userspace(esp3, eip3);
}

} // namespace proc

#undef CHECK_RES
