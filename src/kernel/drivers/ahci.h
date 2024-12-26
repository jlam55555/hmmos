#pragma once

/// \file ahci.h
/// \brief AHCI driver for SATA devices
///
#include <cstddef>
#include <cstdint>
#include <span>

namespace drivers::pci {
struct FuncDescriptor;
}

namespace drivers::ahci {

/// AHCI initialization. Mostly copied from the OSDev examples.
///
/// 1. Standard PCI configuration: enable interrupts, DMA, and memory
///    space access.
/// 2. Memory map the ABAR as uncacheable (ioremap).
/// 3. Enable AHCI mode and interrupts in the AHCI global registers.
/// 4. Loop over implemented ports:
///    a. Allocate and rebase memory regions.
///
/// TODO: assert bios/os handoff not needed
/// TODO: reset controller
/// TODO: register IRQ handler
/// TODO: reset ports
///
bool init(const std::span<const pci::FuncDescriptor> &pci_fn_descriptors);

/// Synchronously read \a count (512-byte) sectors from LBA \a
/// starth:starta to \a buf on port \a port_idx.
///
bool read_blocking(uint8_t port_idx, uint32_t startl, uint32_t starth,
                   uint32_t count, uint16_t *buf);

} // namespace drivers::ahci
