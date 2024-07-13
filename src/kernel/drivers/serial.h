#pragma once

/// Very simple serial driver. Read/write character-by-character using
/// `serial::get().read()` or `serial::get().write(c)`.

#include "asm.h"
#include "perf.h"
#include <assert.h>
#include <stdint.h>

namespace serial {

/// Base IO port addresses for serial ports. COM1 and COM2 are the
/// most standardized, others less so.
enum class Base : uint16_t {
  com1 = 0x3F8,
  com2 = 0x2F8,
  com3 = 0x3E8,
  com4 = 0x2E8,
  com5 = 0x5F8,
  com6 = 0x4F8,
  com7 = 0x5E8,
  com8 = 0x4E8,
};

/// Factory function to construct a Serial.
///
/// This needs to be fwd-declared for the friend declaration.
template <Base com = Base::com1> auto get();

/// A very simple abstraction of a serial device. This is templated by
/// base port address so that the port computations can be constexpr.
///
/// Although if we really cared about performance then `read()` and
/// `write()` would be in pure asm and would be defined in the header
/// so that they are inlinable.
///
/// Since this is stateless (the only state being the base port
/// address) this class is functionally equivalent to a set of
/// templated functions.
template <Base com> class Serial {
public:
  void write(char c) const;
  char read() const;

private:
  bool init() const;
  bool is_transmit_empty() const;
  bool serial_received() const;

  friend auto get<com>();
};

template <Base com> auto get() {
  static bool is_init = false;
  Serial<com> rval;
  if (unlikely(!is_init)) {
    assert(rval.init());
    is_init = true;
  }
  return rval;
}

} // namespace serial
