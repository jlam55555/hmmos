#include "../crt/crt.h"
#include "boot_protocol.h"
#include "drivers/acpi.h"
#include "drivers/ahci.h"
#include "drivers/pci.h"
#include "drivers/serial.h"
#include "fs/drivers/fat32.h"
#include "gdt.h"
#include "idt.h"
#include "mm/kmalloc.h"
#include "mm/page_frame_allocator.h"
#include "mm/virt.h"
#include "nonstd/libc.h"
#include "sched/kthread.h"
#include <climits>
#include <concepts>

static volatile const BP_REQ(MEMORY_MAP, _mem_map_req);

sched::Scheduler *scheduler = nullptr;
void do_schedule() {
  if (likely(scheduler)) {
    scheduler->schedule();
  }
}

namespace {
// Trying out random C++ features.

template <unsigned n> constexpr uint64_t fac() {
  uint64_t res = 1;
  unsigned m = n;
  while (m) {
    res *= m--;
  }
  return res;
}

template <typename T>
concept IsBicycle = requires(T t) {
  { t.year } -> std::same_as<unsigned &>;
};

class A {};
class B {
public:
  unsigned year;
};

static_assert(!IsBicycle<A>, "A is not a bicycle");
static_assert(IsBicycle<B>, "B is a bicycle");

void foo() {
  for (;;) {
    // Cooperative scheduling.
    // nonstd::printf("In thread foo!\r\n");
    // hlt;
    do_schedule();
  }
}

} // namespace

__attribute__((section(".text.entry"))) void _entry() {
  crt::run_global_ctors();

  nonstd::printf("We're in the kernel now!\r\n");

  // C++-ify the memory map.
  struct e820_mm_entry *ent;
  nonstd::printf("Memory map:\r\n");
  uint64_t usable_mem = 0;
  for (ent = _mem_map_req.memory_map; e820_entry_present(ent); ++ent) {
    nonstd::printf("base=0x%llx len=0x%llx type=%u\r\n", ent->base, ent->len,
                   ent->type);
    if (ent->type == E820_MM_TYPE_USABLE) {
      usable_mem += ent->len;
    }
  }
  std::span<e820_mm_entry> mem_map{
      _mem_map_req.memory_map,
      static_cast<size_t>(ent - _mem_map_req.memory_map)};
  nonstd::printf("Found %u entries in the memory map. Usable=0x%llx\r\n",
                 mem_map.size(), usable_mem);

#ifdef DEBUG
  mem::virt::enumerate_page_tables();
#endif

  /// Random computation.
  nonstd::printf("fac(15)=%llu\r\n", fac<15>());
  nonstd::printf("LLONG_MIN=%lld\r\n", LLONG_MIN);

  /// Print to serial.
  const char *str = "Cereal hello world!\r\n";
  while (*str) {
    serial::get().write(*str++);
  }

  arch::gdt::init();

  nonstd::printf("Initializing PFT...\r\n");
  mem::phys::PageFrameTable pft(mem_map);
  nonstd::printf("Total mem=%llx Usable mem=%llx\r\n", pft.total_mem_bytes,
                 pft.usable_mem_bytes);

  nonstd::printf("Initializing PFA...\r\n");
  mem::phys::SimplePFA simple_allocator{pft, 0, pft.mem_limit()};

  mem::set_pfa(&simple_allocator); // for simple kmalloc

  nonstd::printf("Enabling interrupts...\r\n");
  arch::idt::init();

  nonstd::printf("PCI functions:\r\n");
  const auto pci_fn_descs = drivers::pci::enumerate_functions();
  for (const auto &fn_desc : pci_fn_descs) {
    nonstd::printf("%x:%x.%u [%x]: [%x:%x]\r\n", fn_desc.bus, fn_desc.device,
                   fn_desc.function, fn_desc._class, fn_desc.vendor_id,
                   fn_desc.device_id);
  }

  // This depends on PCI
  nonstd::printf("Initializing AHCI driver...\r\n");
  assert(drivers::ahci::init(pci_fn_descs));

  nonstd::printf("Initializing FAT filesystem...\r\n");
  auto boot_part_desc = fs::fat32::Filesystem::find_boot_part();
  assert(boot_part_desc.has_value());
  auto filesystem = fs::fat32::Filesystem::from_partition(*boot_part_desc);

  auto kernel_file = filesystem.lookup_file(filesystem.root_fd(), "KERNEL.BIN");
  assert(kernel_file);
  nonstd::printf("kernel file size=%u\r\n", kernel_file->file_sz_bytes);

  auto src_file = filesystem.lookup_file(filesystem.root_fd(),
                                         "SRC/KERNEL/FS/DRIVERS/FAT32.CC");
  assert(src_file);
  nonstd::printf("contents of %s (size=%u):\r\n", src_file->name,
                 src_file->file_sz_bytes);
  ssize_t rval, pos = 0;
  auto _buf = reinterpret_cast<std::byte *>(mem::kmalloc(4096));
  std::span buf{_buf, 4095};
  while ((rval = filesystem.read(*src_file, pos, buf)) > 0) {
    // Force the string to be null-terminated. We do own the byte past
    // the end of the buffer so this is safe.
    //
    // TODO: we should support a print specifier to print a given
    // number of characters (super useful for string_view as well).
    buf[rval] = std::byte{0};
    nonstd::printf("%s", (const char *)buf.data());
    pos += rval;
  }
  if (rval < 0) {
    nonstd::printf("error reading src file\r\n");
    assert(false);
  }

#ifdef DEBUG
  filesystem.dump_tree(filesystem.root_fd());
#endif

  nonstd::printf("Initializing scheduler...\r\n");
  sched::Scheduler scheduler;
  ::scheduler = &scheduler;
  scheduler.bootstrap();

  // TODO: idle task
  scheduler.new_thread(&foo);

  nonstd::printf("Done.\r\n");

#if 0
  // We should never get here if we call `destroy_thread()` above.
  // scheduler.destroy_thread();
  // nonstd::printf("Unreachable");

  // This should round-robin between main and foo.
  for (;;) {
    // nonstd::printf("In thread main!\r\n");
    // hlt;
    do_schedule();
  }
#endif

  // We'll never reach here unless you modify the above loop.
  crt::run_global_dtors();
  acpi::shutdown();
}
