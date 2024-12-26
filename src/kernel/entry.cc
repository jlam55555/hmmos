#include "../crt/crt.h"
#include "boot_protocol.h"
#include "drivers/acpi.h"
#include "drivers/ahci.h"
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

  // Compute sum of bytes in first sector; compare to
  // $ python3 -c "print(sum(open('out/disk.bin','rb').read(512)))"
  uint8_t *outbuf = reinterpret_cast<uint8_t *>(::operator new(512));
  assert(outbuf != nullptr);
  assert(drivers::ahci::read_blocking(/*port_idx=*/0, /*lba_lo=*/0,
                                      /*lba_hi=*/0,
                                      /*sectors_count=*/1, (uint16_t *)outbuf));
  unsigned sum = 0;
  for (int i = 0; i < 32; ++i) {
    nonstd::printf("%x: ", i << 4);
    for (int j = 0; j < 16; ++j) {
      nonstd::printf("%x ", outbuf[i * 16 + j]);
    }
    nonstd::printf("\r\n");
  }
  for (int i = 0; i < 512; ++i) {
    sum += outbuf[i];
  }
  nonstd::printf("read first sector sum=%u\r\n", sum);

  nonstd::memset(outbuf, 0, 512);
  drivers::ahci::read_blocking(0, 2048, 0, 1, (uint16_t *)outbuf);
  for (int i = 0; i < 32; ++i) {
    nonstd::printf("%x: ", i << 4);
    for (int j = 0; j < 16; ++j) {
      nonstd::printf("%x ", outbuf[i * 16 + j]);
    }
    nonstd::printf("\r\n");
  }

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
