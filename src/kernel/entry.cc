#include "boot_protocol.h"
#include "drivers/serial.h"
#include "nonstd/libc.h"
#include <concepts>
#include <limits.h>

static volatile const BP_REQ(MEMORY_MAP, _mem_map_req);

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

} // namespace

__attribute__((section(".text.entry"))) void _entry() {
  nonstd::printf("We're in the kernel now!\r\n");

  struct e820_mm_entry *ent;
  for (ent = _mem_map_req.memory_map; e820_entry_present(ent); ++ent) {
  }
  nonstd::printf("Found %u entries in the memory map.\r\n",
                 ent - _mem_map_req.memory_map);

  /// Random computation.
  nonstd::printf("fac(15)=%llu\r\n", fac<15>());
  nonstd::printf("LLONG_MIN=%lld\r\n", LLONG_MIN);

  /// Print to serial.
  const char *str = "Cereal hello world!\r\n";
  while (*str) {
    serial::get().write(*str++);
  }

  for (;;) {
    __asm__("hlt");
  }
}
