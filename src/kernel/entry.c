#include "boot_protocol.h"
#include "libc.h"
#include <limits.h>

static volatile const BP_REQ(MEMORY_MAP, _mem_map_req);

static uint64_t _fac(unsigned n) {
  uint64_t res = 1;
  while (n) {
    res *= n--;
  }
  return res;
}

__attribute__((section(".text.entry"))) void _entry() {
  printf("We're in the kernel now!\r\n");

  struct e820_mm_entry *ent;
  for (ent = _mem_map_req.memory_map; e820_entry_present(ent); ++ent) {
  }
  printf("Found %u entries in the memory map.\r\n",
         ent - _mem_map_req.memory_map);

  /// Random computation.
  printf("fac(15)=%llu\r\n", _fac(15));
  printf("LLONG_MIN=%lld\r\n", LLONG_MIN);

  for (;;) {
    __asm__("hlt");
  }
}
