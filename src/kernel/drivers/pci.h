#pragma once

/// \file pci.h
/// \brief Driver for the PCI (Peripheral Component Interconnect) bus
///
/// For simplicity, this doesn't use the new features in PCIe
/// (primarily the memory-mapped configuration space). PCIe is
/// backward-compatible so this should still work.
///
#include "asm.h"
#include "nonstd/libc.h"
#include <cassert>
#include <cstdint>
#include <span>

namespace drivers::pci {

/// PCI(e) topology: a bus contains multiple devices, each device may
/// have one or more functions. Each function may have its own
/// configuration space.

/// Maximum 256 PCI(e) buses.
constexpr unsigned max_buses = 256;

/// Maximum 32 devices per bus.
constexpr unsigned max_devices = 32;

/// Maximum 8 functions per device.
constexpr unsigned max_functions = 8;

/// 64 32-bit registers per function configuration space.
constexpr unsigned max_registers = 256 / 4;

/// Configuration ports.
enum class Port : uint16_t {
  config_addr = 0x0CF8,
  config_data = 0x0CFC,
};

/// \brief Read 32-bit register from function's configuration space.
///
/// \param reg 32-bit register index (not offset in bytes)
///
/// TODO: read configuration space via PCIe memory map
///
/// Layout of the the common configuration space header:
///
/// | Offset | Len | Description     |
/// | ------ | --- | --------------- |
/// | 0x00   | 2   | Vendor ID       |
/// | 0x02   | 2   | Device ID       |
/// | 0x04   | 2   | Command         |
/// | 0x06   | 2   | Status          |
/// | 0x08   | 2   | Revision ID     |
/// | 0x09   | 2   | Prog IF         |
/// | 0x0A   | 2   | Class:Subclass  |
/// | 0x0C   | 1   | Cache Line Size |
/// | 0x0D   | 1   | Latency Timer   |
/// | 0x0E   | 1   | Header Type     |
/// | 0x0F   | 1   | BIST            |
///
uint32_t read_config_register(uint8_t bus, uint8_t device, uint8_t function,
                              uint8_t reg);

/// \brief Read 16-bit word from function's configuration space.
///
/// \param offset Offset in bytes of word from start of configuration space.
uint16_t read_config_word(uint8_t bus, uint8_t device, uint8_t function,
                          uint8_t offset);

/// \brief Read byte from function's configuration space.
///
/// \param offset Offset in bytes from start of configuration space.
uint8_t read_config_byte(uint8_t bus, uint8_t device, uint8_t function,
                         uint8_t offset);

// Helper functions to get particular words.
inline uint16_t get_vendor_id(uint8_t bus, uint8_t device, uint8_t function) {
  return read_config_word(bus, device, function, 0x00);
}
inline uint16_t get_device_id(uint8_t bus, uint8_t device, uint8_t function) {
  return read_config_word(bus, device, function, 0x02);
}
inline uint16_t get_class(uint8_t bus, uint8_t device, uint8_t function) {
  return read_config_word(bus, device, function, 0xA);
}
inline uint8_t get_header_type(uint8_t bus, uint8_t device, uint8_t function) {
  return read_config_byte(bus, device, function, 0x0E);
}
inline uint8_t get_secondary_bus(uint8_t bus, uint8_t device,
                                 uint8_t function) {
  // Precondition: This is a PCI bridge device (header type 0x1).
  return read_config_byte(bus, device, function, 0x19);
}

/// Abbreviated summary of the PCI header returned from enumerating
/// the PCI bus.
struct FuncDescriptor {
  uint8_t bus;
  uint8_t device : 5;
  uint8_t function : 3;

  uint16_t _class;
  uint16_t vendor_id;
  uint16_t device_id;
};

/// Maximum number of PCI functions to enumerate. This number is
/// arbitrary and can be bumped if necessary.
constexpr unsigned max_enumerated_functions = 256;

/// Enumerates and returns the set of PCI functions (up to \a
/// max_enumerated_functions). Repeated calls will return the cached
/// result.
std::span<const FuncDescriptor> enumerate_functions();

} // namespace drivers::pci
