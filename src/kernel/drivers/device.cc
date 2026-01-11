#include "device.h"
#include "drivers/ahci.h"
#include "memdefs.h"
#include "nonstd/libc.h"
#include <cassert>

namespace drivers {

bool BlockDevice::read_page_direct(uint64_t paddr, uint64_t off) {
  // TODO: support different devices. This simply reads from the first
  // AHCI port.
  assert(PG_ALIGNED(paddr) && PG_ALIGNED(off));
  const uint64_t sector = off / 512;
  const uint32_t startl = sector;
  const uint32_t starth = sector >> 32;
  return drivers::ahci::read_blocking(0, startl, starth, PG_SZ / 512, paddr);
}

} // namespace drivers
