#pragma once

/// \file device.h
/// \brief Model of a physical device

#include <cstddef>
#include <cstdint>

namespace drivers {

// NOCOMMIT: think this through a bit more
// Probably should subclass for different types of devices
class Device {};

class BlockDevice : public Device {
public:
  /// Read page from block offset \a off to physical memory address \a
  /// dest. If the page is not mapped into virtual memory, this can
  /// avoid a map+unmap roundtrip.
  virtual bool read_page_direct(uint64_t paddr, uint64_t off);

#if 0
  // We can implement this if it's useful to anyone. Right now only
  // the page cache needs to populate a page directly, and it knows
  // the physical address to populate.

  // Read page from block offset \a off to virtual memory address \a
  // dest.
  virtual bool read_page(std::byte *dest, uint64_t off);
#endif
};

} // namespace drivers
