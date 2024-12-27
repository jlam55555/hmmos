#include "pci.h"

namespace drivers::pci {

uint32_t read_config_register(uint8_t bus, uint8_t device, uint8_t function,
                              uint8_t reg) {
  assert(device < max_devices);
  assert(function < max_functions);
  assert(reg < max_registers);

  uint32_t config_addr = 0x8000'0000U | ((uint32_t)bus << 16) |
                         ((uint32_t)device << 11) | ((uint32_t)function << 8) |
                         ((uint32_t)reg << 2);

  outl((uint16_t)Port::config_addr, config_addr);
  return inl((uint16_t)Port::config_data);
}

uint16_t read_config_word(uint8_t bus, uint8_t device, uint8_t function,
                          uint8_t offset) {
  assert((offset & 0x01) == 0);
  uint32_t reg = read_config_register(bus, device, function, offset >> 2);
  return (reg >> ((offset & 0x02) * 8)) & 0xFFFF;
}

uint8_t read_config_byte(uint8_t bus, uint8_t device, uint8_t function,
                         uint8_t offset) {
  uint32_t reg = read_config_register(bus, device, function, offset >> 2);
  return (reg >> ((offset & 0x03) * 8)) & 0xFF;
}

namespace {
void enumerate_bus(uint8_t bus, FuncDescriptor *enumerated_res,
                   unsigned &enumerated_count);

void enumerate_device(uint8_t bus, uint8_t device,
                      FuncDescriptor *enumerated_res,
                      unsigned &enumerated_count) {
  assert(device < max_devices);

  for (uint8_t function = 0; function < max_functions; ++function) {
    uint16_t vendor_id = get_vendor_id(bus, device, function);
    if (vendor_id == 0xFFFF) {
      if (function == 0) {
        // Device doesn't exist. We assume the first function to be
        // present for each present device.
        return;
      }
      // This function doesn't exist, but functions can be
      // non-contiguous.
      continue;
    }

    // If this fires because we have too many PCI functions, you can
    // bump max_enumerated_functions.
    assert(enumerated_count < max_enumerated_functions);

    FuncDescriptor &fn_desc = enumerated_res[enumerated_count++];
    fn_desc.bus = bus;
    fn_desc.device = device;
    fn_desc.function = function;
    fn_desc.vendor_id = vendor_id;
    fn_desc._class = get_class(bus, device, function);
    fn_desc.device_id = get_device_id(bus, device, function);

    // If this is a PCI bridge device, recursively enumerate the
    // secondary (connected) bus.
    //
    // TODO: make class numbers an enum
    if (fn_desc._class == 0x0604) {
      // Assert that this is a PCI bus type.
      assert((get_header_type(bus, device, function) & 0x7F) == 0x1);
      enumerate_bus(get_secondary_bus(bus, device, function), enumerated_res,
                    enumerated_count);
    }

    if ((get_header_type(bus, device, function) & 0x80) == 0) {
      // Not a multi-function device; break.
      break;
    }
  }
}

void enumerate_bus(uint8_t bus, FuncDescriptor *enumerated_res,
                   unsigned &enumerated_count) {
  for (uint8_t device = 0; device < max_devices; ++device) {
    enumerate_device(bus, device, enumerated_res, enumerated_count);
  }
}
} // namespace

std::span<const FuncDescriptor> enumerate_functions() {
  // Avoiding dynamic allocation here, though we don't really have to.
  static drivers::pci::FuncDescriptor enumerated_res[max_enumerated_functions];
  static unsigned enumerated_count = 0;
  if (enumerated_count > 0) {
    return {enumerated_res, enumerated_count};
  }

  // TODO: check that PCI is supported. For now we assume it is
  // supported (which is a fairly good assumption within the last two
  // decades).

  // Check host bridge (device 0:0). It should have one function for
  // each host controller (which corresponds to a bus where bus ID ==
  // function ID).
  constexpr uint8_t host_bridge_bus = 0;
  constexpr uint8_t host_bridge_device = 0;
  bool multi_bus =
      get_header_type(host_bridge_bus, host_bridge_device, 0) & 0x80;
  for (int host_bridge_function = 0; host_bridge_function < max_functions;
       ++host_bridge_function) {
    if (get_vendor_id(host_bridge_bus, host_bridge_device,
                      host_bridge_function) == 0xFFFF) {
      continue;
    }

    // Bus number is equal to the host bridge function number.
    enumerate_bus(host_bridge_function, enumerated_res, enumerated_count);

    if (!multi_bus) {
      break;
    }
  }
  return {enumerated_res, enumerated_count};
}

} // namespace drivers::pci
