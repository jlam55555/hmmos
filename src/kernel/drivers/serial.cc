#include "serial.h"

#include <cstdint>
#include <stddef.h>

namespace serial {

enum class Offset : uint16_t {
  rxtx_buf = 0,         // DLAB=0
  interrupt_enable = 1, // DLAB=0
  divisor_lsb = 0,      // DLAB=1
  divisor_msb = 1,      // DLAB=1
  interrupt_id_fifo_ctl = 2,
  line_ctl = 3,
  modem_ctl = 4,
  line_status = 5,
  modem_status = 6,
  scratch = 7,
};

constexpr uint16_t get_io_port(Base base, Offset offset) {
  return (uint16_t)base + (uint16_t)offset;
}

template <Base com> bool Serial<com>::init() const {
  // Disable all interrupts.
  outb(get_io_port(com, Offset::interrupt_enable), 0x00);

  // Enable DLAB and set divisor to 3 (38400 baud).
  outb(get_io_port(com, Offset::line_ctl), 0x80);
  outb(get_io_port(com, Offset::divisor_lsb), 0x03);
  outb(get_io_port(com, Offset::divisor_msb), 0x00);

  // 8 bits, no parity, one stop bits
  outb(get_io_port(com, Offset::line_ctl), 0x03);

  // Enable FIFO, clear them, with 14-byte threshold.
  outb(get_io_port(com, Offset::interrupt_id_fifo_ctl), 0xC7);

  // IRQs enabled, RTS/DSR set.
  outb(get_io_port(com, Offset::modem_ctl), 0x0B);

  // Set in loopback mode, test the serial chip. Send byte 0xAE and
  // check if serial returns same byte.
  outb(get_io_port(com, Offset::modem_ctl), 0x1E);
  constexpr uint8_t magic_byte = 0xAE;
  outb(get_io_port(com, Offset::rxtx_buf), magic_byte);

  // Check if serial is faulty (i.e: not same byte as sent)
  if (inb(get_io_port(com, Offset::rxtx_buf)) != magic_byte) {
    return false;
  }

  // If serial is not faulty set it in normal operation mode
  // (not-loopback with IRQs enabled and OUT#1 and OUT#2 bits enabled)
  outb(get_io_port(com, Offset::modem_ctl), 0x0F);
  return true;
}

template <Base com> bool Serial<com>::is_transmit_empty() const {
  return inb(get_io_port(com, Offset::line_status)) & 0x20;
}

template <Base com> void Serial<com>::write(char c) const {
  while (!is_transmit_empty()) {
  }
  outb(get_io_port(com, Offset::rxtx_buf), c);
}

template <Base com> bool Serial<com>::serial_received() const {
  return inb(get_io_port(com, Offset::line_status)) & 0x01;
}

template <Base com> char Serial<com>::read() const {
  while (!serial_received()) {
  }
  return inb(get_io_port(com, Offset::rxtx_buf));
}

template class Serial<Base::com1>;
template class Serial<Base::com2>;
template class Serial<Base::com3>;
template class Serial<Base::com4>;
template class Serial<Base::com5>;
template class Serial<Base::com6>;
template class Serial<Base::com7>;
template class Serial<Base::com8>;

} // namespace serial
