#include "../crt/crt.h"
#include "boot_protocol.h"
#include "console.h"
#include "drivers/acpi.h"
#include "drivers/ahci.h"
#include "drivers/pci.h"
#include "drivers/serial.h"
#include "fs/drivers/fat32.h"
#include "fs/vfs.h"
#include "gdt.h"
#include "idt.h"
#include "mm/kmalloc.h"
#include "mm/page_frame_allocator.h"
#include "mm/virt.h"
#include "nonstd/libc.h"
#include "proc/process.h"
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
proc::Process *curr_proc() {
  if (likely(scheduler)) {
    return scheduler->curr_proc();
  }
  return nullptr;
}

namespace {

__attribute__((noreturn)) void entry() {
  console_use_hhdm();

  nonstd::printf("We're in the kernel now!\r\n");

  // C++-ify the memory map.
  struct e820_mm_entry *ent;
  nonstd::printf("Memory map:\r\n");
  uint64_t usable_mem = 0;
  for (ent = _mem_map_req.memory_map; e820_entry_present(ent); ++ent) {
    nonstd::printf("\tbase=0x%llx len=0x%llx type=%u\r\n", ent->base, ent->len,
                   ent->type);
    if (ent->type == E820_MM_TYPE_USABLE) {
      usable_mem += ent->len;
    }
  }
  std::span<e820_mm_entry> mem_map{
      _mem_map_req.memory_map,
      static_cast<size_t>(ent - _mem_map_req.memory_map)};
  nonstd::printf("\tFound %u entries in the memory map. Usable=0x%llx\r\n",
                 mem_map.size(), usable_mem);

#ifdef DEBUG
  mem::virt::enumerate_page_tables();
#endif

  nonstd::printf("Initializing kernel GDT...\r\n");
  arch::gdt::init();

  nonstd::printf("Initializing PFT...\r\n");
  mem::phys::PageFrameTable pft(mem_map);
  nonstd::printf("\tTotal mem=%llx Usable mem=%llx\r\n", pft.total_mem_bytes,
                 pft.usable_mem_bytes);

  nonstd::printf("Initializing PFA...\r\n");
  mem::phys::SimplePFA simple_allocator{pft, 0, pft.mem_limit()};

  mem::set_pfa(&simple_allocator); // for simple kmalloc

  nonstd::printf("Enabling interrupts...\r\n");
  arch::idt::init();

  nonstd::printf("PCI functions:\r\n");
  const auto pci_fn_descs = drivers::pci::enumerate_functions();
  for (const auto &fn_desc : pci_fn_descs) {
    nonstd::printf("\t%x:%x.%u [%x]: [%x:%x]\r\n", fn_desc.bus, fn_desc.device,
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
  fs::init(filesystem);

  nonstd::printf("Initializing scheduler...\r\n");
  sched::Scheduler scheduler;
  ::scheduler = &scheduler;
  scheduler.bootstrap();

  nonstd::printf("Spawning the init process...\r\n");
  fs::Result res{};
  new proc::Process(scheduler, "/BIN/INIT", res);
  ASSERT(res == fs::Result::Ok);

  // This becomes the idle task.
  for (;;) {
    hlt;
  }
}

} // namespace

__attribute__((section(".text.entry"), noreturn)) void _entry() {
  crt::run_global_ctors();
  entry();

  // We never reach here on normal operation.
  //
  // TODO: we need a "proper shutdown sequence" that does call the
  // global dtors.
  __builtin_unreachable();
  ASSERT(false);

#if 0
  crt::run_global_dtors();
  acpi::shutdown();
#endif
}
