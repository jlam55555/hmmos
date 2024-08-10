#include "pic.h"

namespace {

enum class port : uint8_t {
  pic1_cmd = 0x20,
  pic2_cmd = 0xA0,
  pic1_data = pic1_cmd + 1,
  pic2_data = pic2_cmd + 1,
};

/// End-of-interrupt command code
constexpr uint8_t end_of_interrupt = 0x20;

/// The initialization process comprises of sending the initialization
/// command (0x11), followed by three other "Initialization Command
/// Words" (ICW2, ICW3, ICW4).
enum class init : uint8_t {
  /// Start initialization sequence with ICW2-4.
  icw1_icw4 = 0x11,

  /// Single (cascade) mode.
  icw1_single = 0x12,

  /// Call address interval 4 (8).
  icw1_interval4 = 0x14,

  /// Level-triggered (edge) mode.
  icw1_level = 0x18,

  /// 8086/88 (MCS-80/85) mode.
  icw4_8086 = 0x01,

  /// Auto (normal) EOI.
  icw4_auto = 0x02,

  /// Buffered mode/slave.
  icw4_slave = 0x08,

  /// Buffered mode/master.
  icw4_master = 0x0C,
};

/// Perform a small delay. See
/// https://wiki.osdev.org/Inline_Assembly/Examples#IO_WAIT.
void io_wait() { outb(0x80, 0); }

// Helper functions for strongly-typed enums.
void outb(port port, uint8_t val) { return ::outb((uint8_t)port, val); }
void outb(port port, init val) { return ::outb((uint8_t)port, (uint8_t)val); }
uint8_t inb(port port) { return ::inb((uint8_t)port); }

} // namespace

namespace drivers::pic {

void disable() {
  outb(port::pic1_data, 0xFF);
  outb(port::pic2_data, 0xFF);
}

void eoi(uint8_t irq) {
  if (irq >= 8) {
    outb(port::pic2_cmd, end_of_interrupt);
  }
  outb(port::pic1_cmd, end_of_interrupt);
}

void init(uint8_t offset1, uint8_t offset2) {
  // Save masks.
  uint8_t a1 = inb(port::pic1_data);
  uint8_t a2 = inb(port::pic2_data);

  // Starts the initialization sequence (in cascade mode)
  outb(port::pic1_cmd, init::icw1_icw4);
  io_wait();
  outb(port::pic2_cmd, init::icw1_icw4);
  io_wait();

  // ICW2: Master PIC vector offset.
  outb(port::pic1_data, offset1);
  io_wait();
  // ICW2: Slave PIC vector offset.
  outb(port::pic2_data, offset2);
  io_wait();

  // ICW3: tell Master PIC that there is a slave PIC at IRQ2 (0000 0100).
  outb(port::pic1_data, 4);
  io_wait();
  // ICW3: tell Slave PIC its cascade identity (0000 0010).
  outb(port::pic2_data, 2);
  io_wait();

  // ICW4: have the PICs use 8086 mode (and not 8080 mode).
  outb(port::pic1_data, init::icw4_8086);
  io_wait();
  outb(port::pic2_data, init::icw4_8086);
  io_wait();

  // Restore saved masks.
  outb(port::pic1_data, a1);
  outb(port::pic2_data, a2);
}

} // namespace drivers::pic
