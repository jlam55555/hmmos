#include "../crt/crt.h"
#include "boot_protocol.h"
#include "drivers/acpi.h"
#include "drivers/pci.h"
#include "drivers/serial.h"
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
  for (ent = _mem_map_req.memory_map; e820_entry_present(ent); ++ent) {
  }
  std::span<e820_mm_entry> mem_map{
      _mem_map_req.memory_map,
      static_cast<size_t>(ent - _mem_map_req.memory_map)};
  nonstd::printf("Found %u entries in the memory map.\r\n", mem_map.size());

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
  for (const auto &fn_desc : drivers::pci::enumerate_functions()) {
    nonstd::printf("%x:%x.%u [%x]: [%x:%x]\r\n", fn_desc.bus, fn_desc.device,
                   fn_desc.function, fn_desc._class, fn_desc.vendor_id,
                   fn_desc.device_id);
  }

#if 0
  nonstd::printf("Initializing scheduler...\r\n");
  sched::Scheduler scheduler;
  ::scheduler = &scheduler;
  scheduler.bootstrap();

  // TODO: idle task
  scheduler.new_thread(&foo);

  nonstd::printf("Done.\r\n");

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
