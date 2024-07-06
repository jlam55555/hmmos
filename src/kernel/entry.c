#include "boot_protocol.h"
#include "pmode_print.h"

static volatile const BP_REQ(MEMORY_MAP, _mem_map_req);

static uint64_t _fac(unsigned n) {
  uint64_t res = 1;
  while (n) {
    res *= n--;
  }
  return res;
}

__attribute__((section(".text.entry"))) void _entry() {
  pmode_puts("We're in the kernel now!\r\n");

  struct e820_mm_entry *ent;
  for (ent = _mem_map_req.memory_map; e820_entry_present(ent); ++ent) {
  }
  pmode_puts("Found ");
  pmode_printb(ent - _mem_map_req.memory_map);
  pmode_puts(" entries in the memory map.\r\n");

  /// Random computation.
  pmode_puts("fac(15)=");
  pmode_printq(_fac(15));
  pmode_puts("\r\n");

  for (;;) {
    __asm__("hlt");
  }
}
