#include "../crt/crt.h"
#include "boot_protocol.h"
#include "drivers/acpi.h"
#include "drivers/serial.h"
#include "idt.h"
#include "mm/kmalloc.h"
#include "mm/page_frame_allocator.h"
#include "mm/page_frame_table.h"
#include "nonstd/libc.h"
#include "test.h"

static volatile const BP_REQ(MEMORY_MAP, _mem_map_req);

void do_schedule() {}

char test_selection_buf[4096] = {};

__attribute__((section(".text.entry"))) void _entry() {
  crt::run_global_ctors();

  // Any (un)locking requires sti, which requires the IDT to be setup
  // correctly.
  arch::idt::init();

  // TODO: could use scanf() logic
  char c;
  unsigned pos = 0;
  while ((c = serial::get().read()) != '\n' &&
         pos < sizeof test_selection_buf) {
    test_selection_buf[pos++] = c;
  }

  {
    // Set up global heap.
    struct e820_mm_entry *ent;
    for (ent = _mem_map_req.memory_map; e820_entry_present(ent); ++ent) {
    }
    std::span<e820_mm_entry> mem_map{
        _mem_map_req.memory_map,
        static_cast<size_t>(ent - _mem_map_req.memory_map)};
    mem::phys::PageFrameTable pft(mem_map);
    mem::phys::SimplePFA simple_allocator{pft, 0, pft.mem_limit()};
    mem::set_pfa(&simple_allocator); // for simple kmalloc
  }

  test::run_tests(test_selection_buf);

  crt::run_global_dtors();
  acpi::shutdown();
}
