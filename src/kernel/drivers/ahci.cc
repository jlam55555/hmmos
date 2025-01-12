#include "ahci.h"
#include "drivers/pci.h"
#include "libc_minimal.h"
#include "memdefs.h"
#include "mm/virt.h"
#include "nonstd/libc.h"
#include "perf.h"
#include "util/algorithm.h"
#include "util/bw.h"
#include "util/objutil.h"
#include <new>
#include <optional>
#include <type_traits>

namespace drivers::ahci {

namespace {

/// AHCI data structures can all be found here. This is how they're
/// related:
///
/// In PCI configuration space for AHCI device:
/// - BAR 5 points to \ref GlobalRegisters (a.k.a. ABAR)
///
/// In GlobalRegisters:
/// - Contains an array of \ref PortRegisters for each all ports.
///
/// In PortRegisters:
/// - Refers to a command list (array of \ref CommandHeader)
/// - Refers to \ref ReceivedFIS
///
/// In CommandHeader:
/// - Refers to a \ref CommandTable
///
/// In CommandTable:
/// - Contains the ATA command FIS (see \a fis namespace)
/// - Contains the physical region descriptor table (array of \ref
///   PRDTEntry)
///
/// The structs are roughly laid out in topological order (reverse
/// order of what is described above).

/// SATA Frame Information Structure (FIS) packet definitions
namespace fis {

enum class Type : uint8_t {
  RegisterH2D = 0x27,  // Register FIS - host to device
  RegisterD2H = 0x34,  // Register FIS - device to host
  DMAActivate = 0x39,  // DMA activate FIS - device to host
  DMASetup = 0x41,     // DMA setup FIS - bidirectional
  Data = 0x46,         // Data FIS - bidirectional
  BISTActivate = 0x58, // BIST activate FIS - bidirectional
  PIOSetup = 0x5F,     // PIO setup FIS - device to host
  SDB = 0xA1,          // Set device bits FIS - device to host
};

/// ATA opcodes.
///
/// See https://wiki.osdev.org/ATA_Command_Matrix
enum class ATACommand : uint8_t {
  ReadDMAExt = 0x25,
};

struct RegisterH2D {
  // DWORD 0
  Type fis_type = Type::RegisterH2D;

  uint8_t pmport : 4; // Port multiplier
  uint8_t rsv0 : 3;   // Reserved
  uint8_t c : 1;      // 1: Command, 0: Control

  ATACommand command; // Command register
  uint8_t featurel;   // Feature register, 7:0

  // DWORD 1
  uint8_t lba0;   // LBA low register, 7:0
  uint8_t lba1;   // LBA mid register, 15:8
  uint8_t lba2;   // LBA high register, 23:16
  uint8_t device; // Device register

  // DWORD 2
  uint8_t lba3;     // LBA register, 31:24
  uint8_t lba4;     // LBA register, 39:32
  uint8_t lba5;     // LBA register, 47:40
  uint8_t featureh; // Feature register, 15:8

  // DWORD 3
  uint8_t countl;  // Count register, 7:0
  uint8_t counth;  // Count register, 15:8
  uint8_t icc;     // Isochronous command completion
  uint8_t control; // Control register

  // DWORD 4
  uint8_t rsv1[4]; // Reserved
};
static_assert(sizeof(RegisterH2D) == 0x14);

struct RegisterD2H {
  // DWORD 0
  Type fis_type = Type::RegisterD2H;

  uint8_t pmport : 4; // Port multiplier
  uint8_t rsv0 : 2;   // Reserved
  uint8_t i : 1;      // Interrupt bit
  uint8_t rsv1 : 1;   // Reserved

  uint8_t status; // Status register
  uint8_t error;  // Error register

  // DWORD 1
  uint8_t lba0;   // LBA low register, 7:0
  uint8_t lba1;   // LBA mid register, 15:8
  uint8_t lba2;   // LBA high register, 23:16
  uint8_t device; // Device register

  // DWORD 2
  uint8_t lba3; // LBA register, 31:24
  uint8_t lba4; // LBA register, 39:32
  uint8_t lba5; // LBA register, 47:40
  uint8_t rsv2; // Reserved

  // DWORD 3
  uint8_t countl;  // Count register, 7:0
  uint8_t counth;  // Count register, 15:8
  uint8_t rsv3[2]; // Reserved

  // DWORD 4
  uint8_t rsv4[4]; // Reserved
};
static_assert(sizeof(RegisterD2H) == 0x14);

struct Data {
  // DWORD 0
  Type fis_type = Type::Data;

  uint8_t pmport : 4; // Port multiplier
  uint8_t rsv0 : 4;   // Reserved

  uint8_t rsv1[2]; // Reserved

  // DWORD 1 ~ N
  uint32_t data[0]; // Payload
};
static_assert(sizeof(Data) == 0x4);

struct PIOSetup {
  // DWORD 0
  Type fis_type = Type::PIOSetup;

  uint8_t pmport : 4; // Port multiplier
  uint8_t rsv0 : 1;   // Reserved
  uint8_t d : 1;      // Data transfer direction, 1 - device to host
  uint8_t i : 1;      // Interrupt bit
  uint8_t rsv1 : 1;

  uint8_t status; // Status register
  uint8_t error;  // Error register

  // DWORD 1
  uint8_t lba0;   // LBA low register, 7:0
  uint8_t lba1;   // LBA mid register, 15:8
  uint8_t lba2;   // LBA high register, 23:16
  uint8_t device; // Device register

  // DWORD 2
  uint8_t lba3; // LBA register, 31:24
  uint8_t lba4; // LBA register, 39:32
  uint8_t lba5; // LBA register, 47:40
  uint8_t rsv2; // Reserved

  // DWORD 3
  uint8_t countl;   // Count register, 7:0
  uint8_t counth;   // Count register, 15:8
  uint8_t rsv3;     // Reserved
  uint8_t e_status; // New value of status register

  // DWORD 4
  uint16_t tc;     // Transfer count
  uint8_t rsv4[2]; // Reserved
};
static_assert(sizeof(PIOSetup) == 0x14);

struct DMASetup {
  // DWORD 0
  Type fis_type = Type::DMASetup;

  uint8_t pmport : 4; // Port multiplier
  uint8_t rsv0 : 1;   // Reserved
  uint8_t d : 1;      // Data transfer direction, 1 - device to host
  uint8_t i : 1;      // Interrupt bit
  uint8_t a : 1;      // Auto-activate. Specifies if DMA Activate FIS is needed

  uint8_t rsved[2]; // Reserved

  // DWORD 1&2

  uint64_t DMAbufferID; // DMA Buffer Identifier. Used to Identify DMA buffer in
                        // host memory. SATA Spec says host specific and not in
                        // Spec. Trying AHCI spec might work.

  // DWORD 3
  uint32_t rsvd; // More reserved

  // DWORD 4
  uint32_t DMAbufOffset; // Byte offset into buffer. First 2 bits must be 0

  // DWORD 5
  uint32_t TransferCount; // Number of bytes to transfer. Bit 0 must be 0

  // DWORD 6
  uint32_t resvd; // Reserved
};
static_assert(sizeof(DMASetup) == 0x1C);

struct SetDeviceBits {
  // DWORD 0
  Type fis_type = Type::SDB;
  uint8_t rsvd0 : 6;
  uint8_t i : 1;
  uint8_t rsvd1 : 1;

  uint8_t status_lo : 3;
  uint8_t rsvd2 : 1;
  uint8_t status_hi : 3;
  uint8_t rsvd3 : 1;
  uint8_t error;

  // DWORD 1
  uint32_t rsvd4;
};
static_assert(sizeof(SetDeviceBits) == 0x08);

} // namespace fis

/// Signatures for different port types.
///
/// \see \a PortRegisters::sig
enum class DeviceSignature : uint32_t {
  ATA = 0x00000101,
  ATAPI = 0xEB140101,
  EnclosureManagementBridge = 0xC33C0101,
  PortMultiplier = 0x96690101,
};

namespace detail {

/// \brief Memory-mapped region for an AHCI port.
///
/// This may be DMA-ed by the device and must be marked volatile.
struct PortRegistersImpl {
  uint32_t clb;          // 0x00, command list base address, 1K-byte aligned
  uint32_t clbu;         // 0x04, command list base address upper 32 bits
  uint32_t fb;           // 0x08, FIS base address, 256-byte aligned
  uint32_t fbu;          // 0x0C, FIS base address upper 32 bits
  struct {               //
    uint8_t dhrs : 1;    //
    uint8_t pss : 1;     //
    uint8_t dss : 1;     //
    uint8_t sdbs : 1;    //
    uint8_t ufs : 1;     //
    uint8_t dps : 1;     //
    uint8_t pcs : 1;     //
    uint8_t dmps : 1;    //
    uint8_t rsvd0 : 8;   //
    uint8_t rsvd1 : 6;   //
    uint8_t prcs : 1;    //
    uint8_t ipms : 1;    //
    uint8_t ofs : 1;     //
    uint8_t rsvd2 : 1;   //
    uint8_t infs : 1;    //
    uint8_t ifs : 1;     //
    uint8_t hbds : 1;    //
    uint8_t hbfs : 1;    //
    uint8_t tfes : 1;    //
    uint8_t cpds : 1;    //
  } is;                  // 0x10, interrupt status
  uint32_t ie;           // 0x14, interrupt enable
  struct {               //
    uint8_t st : 1;      // start
    uint8_t sud : 1;     //
    uint8_t pod : 1;     //
    uint8_t clo : 1;     //
    uint8_t fre : 1;     // FIS receive enable
    uint8_t rsvd0 : 3;   //
    uint8_t ccs : 5;     //
    uint8_t mpss : 1;    //
    uint8_t fr : 1;      // FIS receive running
    uint8_t cr : 1;      // command list running
    uint8_t cps : 1;     //
    uint8_t pma : 1;     //
    uint8_t hpcp : 1;    //
    uint8_t mpsp : 1;    //
    uint8_t cpd : 1;     //
    uint8_t esp : 1;     //
    uint8_t fbscp : 1;   //
    uint8_t apste : 1;   //
    uint8_t atapi : 1;   //
    uint8_t dlae : 1;    //
    uint8_t alpe : 1;    //
    uint8_t asp : 1;     //
    uint8_t icc : 4;     //
  } cmd;                 // 0x18, command and status
  uint32_t rsv0;         // 0x1C, Reserved
  struct {               //
    uint8_t sts_err : 1; //
    uint8_t cs0 : 2;     //
    uint8_t drq : 1;     //
    uint8_t cs1 : 3;     //
    uint8_t bsy : 1;     //
    uint8_t err;         //
    uint16_t rsvd;       //
  } tfd;                 // 0x20, task file data
  DeviceSignature sig;   // 0x24, signature
  struct {               // 0x28, SATA status (SCR0:SStatus)
    uint8_t det : 4;     //       interface power management
    uint8_t spd : 4;     //       current interface speed
    uint8_t ipm : 4;     //       device detection
    uint32_t rsvd : 20;  //
  } ssts;                //
  uint32_t sctl;         // 0x2C, SATA control (SCR2:SControl)
  uint32_t serr;         // 0x30, SATA error (SCR1:SError)
  uint32_t sact;         // 0x34, SATA active (SCR3:SActive)
  uint32_t ci;           // 0x38, command issue
  uint32_t sntf;         // 0x3C, SATA notification (SCR4:SNotification)
  uint32_t fbs;          // 0x40, FIS-based switch control
  uint32_t rsv1[11];     // 0x44 ~ 0x6F, Reserved
  uint32_t vendor[4];    // 0x70 ~ 0x7F, vendor specific
};

/// \brief Memory-mapped region for the AHCI HBA.
///
/// This can be found via the AHCI Base Address Register (ABAR,
/// a.k.a. BAR5 in the AHCI PCI configuration space.)
///
/// This may be DMA-ed by the device and must be marked volatile.
struct GlobalRegistersImpl {
  // 0x00 - 0x2B, Generic Host Control
  uint32_t cap;     // 0x00, Host capability
  uint32_t ghc;     // 0x04, Global host control
  uint32_t is;      // 0x08, Interrupt status
  uint32_t pi;      // 0x0C, Port implemented
  uint32_t vs;      // 0x10, Version
  uint32_t ccc_ctl; // 0x14, Command completion coalescing control
  uint32_t ccc_pts; // 0x18, Command completion coalescing ports
  uint32_t em_loc;  // 0x1C, Enclosure management location
  uint32_t em_ctl;  // 0x20, Enclosure management control
  uint32_t cap2;    // 0x24, Host capabilities extended
  uint32_t bohc;    // 0x28, BIOS/OS handoff control and status

  // 0x2C - 0x9F, Reserved
  uint8_t rsv[0xA0 - 0x2C];

  // 0xA0 - 0xFF, Vendor specific registers
  uint8_t vendor[0x100 - 0xA0];

  // 0x100 - 0x10FF, Port control registers
  PortRegistersImpl ports[0]; // 1 ~ 32
};

/// \brief Received FIS memory region.
///
/// This may be DMA-ed by the device and must be marked volatile.
struct ReceivedFISImpl {
  // 0x00
  fis::DMASetup dsfis; // DMA Setup FIS
  uint8_t pad0[4];

  // 0x20
  fis::PIOSetup psfis; // PIO Setup FIS
  uint8_t pad1[12];

  // 0x40
  fis::RegisterD2H rfis; // Register – Device to Host FIS
  uint8_t pad2[4];

  // 0x58
  fis::SetDeviceBits sdbfis; // Set Device Bit FIS

  // 0x60
  uint8_t ufis[64];

  // 0xA0
  uint8_t rsv[0x100 - 0xA0];
};

} // namespace detail

using PortRegisters = volatile detail::PortRegistersImpl;
static_assert(sizeof(PortRegisters) == 0x80);
using GlobalRegisters = volatile detail::GlobalRegistersImpl;
static_assert(sizeof(GlobalRegisters) == 0x100);
using ReceivedFIS = volatile detail::ReceivedFISImpl;
static_assert(sizeof(ReceivedFIS) == 0x100);

struct CommandHeader {
  // DW0
  uint8_t cfl : 5; // Command FIS length in DWORDS, 2 ~ 16
  uint8_t a : 1;   // ATAPI
  uint8_t w : 1;   // Write, 1: H2D, 0: D2H
  uint8_t p : 1;   // Prefetchable

  uint8_t r : 1;    // Reset
  uint8_t b : 1;    // BIST
  uint8_t c : 1;    // Clear busy upon R_OK
  uint8_t rsv0 : 1; // Reserved
  uint8_t pmp : 4;  // Port multiplier port

  uint16_t prdtl; // Physical region descriptor table length in entries

  // DW1
  volatile uint32_t prdbc; // Physical region descriptor byte count transferred

  // DW2, 3
  uint32_t ctba;  // Command table descriptor base address
  uint32_t ctbau; // Command table descriptor base address upper 32 bits

  // DW4 - 7
  uint32_t rsv1[4]; // Reserved
};
static_assert(sizeof(CommandHeader) == 0x20);

struct PRDTEntry {
  uint32_t dba;  // Data base address
  uint32_t dbau; // Data base address upper 32 bits
  uint32_t rsv0; // Reserved

  // DW3
  uint32_t dbc : 22; // Byte count, 4M max
  uint32_t rsv1 : 9; // Reserved
  uint32_t i : 1;    // Interrupt on completion
};
static_assert(sizeof(PRDTEntry) == 0x10);

struct CommandTable {
  // 0x00
  uint8_t cfis[64]; // Command FIS

  // 0x40
  uint8_t acmd[16]; // ATAPI command, 12 or 16 bytes

  // 0x50
  uint8_t rsv[48]; // Reserved

  // 0x80
  PRDTEntry
      prdt_entry[0]; // Physical region descriptor table entries, 0 ~ 65535
};
static_assert(sizeof(CommandTable) == 0x80);

/// Print some diagnostic information when scanning SATA devices.
///
/// \return the number of implemented ports found.
uint8_t enumerate_ports(const GlobalRegisters &abar) {
  // Port implemented mask.
  uint32_t pi = abar.pi;
  uint8_t ports = 0;

  nonstd::printf("AHCI ports:\r\n");
  for (uint32_t mask = 1U, i = 0; mask; mask <<= 1, ++i) {
    if (!(pi & mask)) {
      continue;
    }
    ++ports;

    const auto &port = abar.ports[i];
    auto ssts = objutil::copy_from_volatile(port.ssts);
    const char *type_str;
    if (ssts.det != 0x03 /*=present*/ || ssts.ipm != 0x01 /*=active*/) {
      // No/Inactive device.
      type_str = "No";
    } else {
      switch (port.sig) {
      case DeviceSignature::ATA:
        type_str = "SATA";
        break;
      case DeviceSignature::ATAPI:
        type_str = "SATAPI";
        break;
      case DeviceSignature::EnclosureManagementBridge:
        type_str = "SEMB";
        break;
      case DeviceSignature::PortMultiplier:
        type_str = "PM";
        break;
      default:
        // TODO: need exceptions
        nonstd::printf("invalid port signature 0x%x for device %u\r\n",
                       port.sig, i);
        assert(false);
      }
    }

    nonstd::printf("\t%s drive found at port %d\r\n", type_str, i);
  }
  return ports;
}

// Start command engine
void unpause_cmd_engine(PortRegisters &port) {
  using Cmd = decltype(port.cmd);

  while (bw::and_(port.cmd, Cmd{.cr = 1})) {
  }

  auto tmp = bw::or_<Cmd>(port.cmd, Cmd{.st = 1, .fre = 1});
  objutil::copy_to_volatile(port.cmd, tmp);
}

// Stop command engine
void pause_cmd_engine(PortRegisters &port) {
  using Cmd = decltype(port.cmd);

  auto tmp = bw::and_<Cmd>(port.cmd, bw::not_(Cmd{.st = 1, .fre = 1}));
  objutil::copy_to_volatile(port.cmd, tmp);

  while (bw::and_(port.cmd, Cmd{.fr = 1, .cr = 1})) {
  }
}

/// Number of command slots and command tables allocated per
/// port. AHCI supports up to 32 slots per port.
constexpr unsigned command_slots_per_port = 32;

/// Number of \a PRDTEntry allocated per command. A command table
/// supports up to 2^16 PRDT entries, but realistically we won't
/// need more than one or two per command.
constexpr unsigned prdtes_per_command = 8;

/// Conversions between IO memory mapped addresses (used by code) and
/// physical memory addresses (used by the AHCI device). We can't use
/// the regular HHDM mapping since those are hugepages with caching
/// enabled.
///
/// \ref ahci_virt_base and \ref ahci_phys_base are set during AHCI
/// init.
std::byte *ahci_virt_base = nullptr;
uint64_t ahci_phys_base = 0;
uint64_t ahci_virt_to_phys(std::byte *virt) {
  assert(ahci_phys_base != 0 && ahci_virt_base != nullptr);
  return ahci_phys_base + (virt - ahci_virt_base);
}
std::byte *ahci_phys_to_virt(uint64_t phys) {
  assert(ahci_phys_base != 0 && ahci_virt_base != nullptr);
  return ahci_virt_base + (phys - ahci_phys_base);
}

/// Allocate memory for received FIS structures, command lists,
/// command tables, and PRDTs.
///
/// When setting these values in the port registers, we have to
/// temporarily pause the command engine.
///
/// \note This does not free the original port memory regions set up
/// by the firmware.
///
/// \return false on failure
bool rebase_port_memory(GlobalRegisters &abar, unsigned max_port) {
  // The alignment requirements for data structures are as following:
  // - Command list: 1KB
  // - Received FIS: 256B
  // - Command table: 128B
  //   - Since PRDTs follow command tables, PRDT * prdtes_per_command
  //     must also be 128B aligned
  //
  // To meet these requirements, we'll allocate in order of largest to
  // smallest alignment requirement:
  //
  // - command_slots_per_port * sizeof(CommandList):
  //   1K per entry, array is 1K aligned
  // - command_slots_per_port * sizeof(ReceivedFIS):
  //   256B per entry, array is 1K aligned
  // - command_slots_per_port * (sizeof(CommandTable) +
  //   prdtes_per_command * sizeof(PRDTEntry)):
  //   8KB per entry (if 8 PRDTEs/cmdtbl), array is 256B aligned)
  //
  // Overall this comes out to 9.25KB per port. This will consume IO
  // port virtual address space, since we need to map this memory as
  // uncacheable.
  //
  assert(max_port < 32);
  assert(util::algorithm::aligned_pow2<sizeof(CommandTable)>(
      prdtes_per_command * sizeof(PRDTEntry)));

  // Step 1: allocation
  //
  // We need ~9KB mem per port. Total memory needs to be rounded up to
  // the nearest page size since it needs to be marked uncacheable.
  // The kmalloc implementation is guaranteed to give us page-aligned
  // addresses if allocating more than a page at a time.
  constexpr size_t req_sz_per_port =
      /*sizeof cmdlist=*/1024 + sizeof(ReceivedFIS) +
      command_slots_per_port *
          (sizeof(CommandTable) + prdtes_per_command * sizeof(PRDTEntry));
  static_assert(req_sz_per_port == 1024 + 256 + 8192);
  const size_t req_sz = req_sz_per_port * max_port;
  const size_t alloc_sz = util::algorithm::ceil_pow2<PG_SZ>(req_sz);

  // 1. Allocate space in IO virtual address space.
  // 2. Reserve physical page frames (these don't have to be in the
  //    low 1GB of memory since we won't access these through the
  //    HHDM).
  // 3. Remap the IO virtual addresses to the physical page frames.
  //
  // TODO: add option to reserve memory from low 1GB vs. rest of mem.
  ahci_virt_base =
      static_cast<std::byte *>(mem::virt::io_alloc(alloc_sz >> PG_SZ_BITS));
  if (ahci_virt_base == nullptr) {
    return false;
  }
  auto *base_mem_frames = ::operator new(alloc_sz);
  if (base_mem_frames == nullptr) {
    return false;
  }
  ahci_phys_base = mem::virt::hhdm_to_direct(base_mem_frames);
  if (!mem::virt::ioremap(ahci_phys_base, ahci_virt_base,
                          alloc_sz >> PG_SZ_BITS)) {
    return false;
  }
  nonstd::memset(ahci_virt_base, 0, req_sz);

#ifdef DEBUG
  nonstd::printf(
      "allocating %u memory for AHCI port memory regions base_mem=0x%x\r\n",
      alloc_sz, (size_t)ahci_virt_base);
#endif

  std::byte *received_fis_base = ahci_virt_base + (max_port << 10);
  std::byte *cmd_tbl_base = received_fis_base + max_port * sizeof(ReceivedFIS);
  for (unsigned i = 0; i < max_port; ++i) {
    auto &port = abar.ports[i];

    // Step 2: pause the command engine
    pause_cmd_engine(port);

    // Step 3: Update the port registers to point to the memory regions.
    port.fb = ahci_virt_to_phys(received_fis_base + i * sizeof(ReceivedFIS));
    port.fbu = 0;

    auto *cmd_hdr =
        reinterpret_cast<CommandHeader *>(ahci_virt_base + (i << 10));
    port.clb = ahci_virt_to_phys(reinterpret_cast<std::byte *>(cmd_hdr));
    port.clbu = 0;

    for (unsigned cmd = 0; cmd < command_slots_per_port; ++cmd) {
      // Don't really need to set this here since it'll be set on
      // commands, but might as well.
      cmd_hdr[i].prdtl = prdtes_per_command;

      cmd_hdr[i].ctba = ahci_virt_to_phys(
          cmd_tbl_base + cmd * (sizeof(CommandTable) +
                                prdtes_per_command * sizeof(PRDTEntry)));
      cmd_hdr[i].ctbau = 0;
    }

    // Step 4: unpause the command engine
    unpause_cmd_engine(port);
  }

  return true;
}

/// \return the free command list slot, or -1 if no slots available
int find_free_cmdslot(const PortRegisters &port) {
  // If not set in SACT and CI, the slot is free
  uint32_t slots = (port.sact | port.ci);
  for (uint32_t mask = 1, i = 0; mask; ++i, mask <<= 1) {
    if ((slots & mask) == 0) {
      return i;
    }
  }
  return -1;
}

GlobalRegisters *abar = nullptr;

} // namespace

bool init(const std::span<const pci::FuncDescriptor> &pci_fn_descriptors) {
  std::optional<pci::FuncDescriptor> pci_func_desc;
  for (const auto fn_desc : pci_fn_descriptors) {
    // TODO: also check prog IF byte
    // TODO: add an enum for this
    if (fn_desc._class == 0x0106) {
      pci_func_desc = fn_desc;
      break;
    }
  }
  if (unlikely(!pci_func_desc.has_value())) {
    nonstd::printf("couldn't find AHCI function in PCI device list\r\n");
    return false;
  }

  // Configure PCI device.
  uint16_t command_reg = pci::read_config_word(
      pci_func_desc->bus, pci_func_desc->device, pci_func_desc->function, 0x04);
  command_reg |= 0x02;    // Enable memory space access.
  command_reg |= 0x04;    // Enable DMA (bus-mastering).
  command_reg &= ~0x0400; // Enable interrupts.
  pci::write_config_register(pci_func_desc->bus, pci_func_desc->device,
                             pci_func_desc->function, 0x01, command_reg);

  // Map ABAR physical address into virtual memory (as uncacheable).
  uint32_t abar_phys = pci::get_bar(pci_func_desc->bus, pci_func_desc->device,
                                    pci_func_desc->function, 5);
  abar = reinterpret_cast<GlobalRegisters *>(mem::virt::io_alloc(1));
  if (unlikely(abar == nullptr) ||
      unlikely(!mem::virt::ioremap(abar_phys, (void *)abar, 1))) {
    return false;
  }

  // Configure GHC in the AHCI global registers.
  uint32_t ghc = abar->ghc;
  ghc |= 0x80000000; // Enable AHCI mode.
  ghc |= 0x02;       // Enable interrupts.
  abar->ghc = ghc;

  // Get highest set bit (highest port index) from "port implemented"
  // register.
  uint32_t pi = abar->pi;
  uint8_t max_port = pi ? 32 - __builtin_clz(pi) : 0;

  // The AHCI memory region can contain up to 32 ports. The
  // GlobalRegisters portion has size 0x100; each port has size 0x80
  // (up to a maximum of 0x1000 total). If port 31 or 32 is
  // implemented, this will exceed the above single page ioremap-ed
  // above. We could use two pages above but that might be
  // unnecessarily wasteful.
  size_t abar_sz =
      ((char *)&abar->ports[max_port] + sizeof(abar->ports[max_port])) -
      (char *)abar;
  assert(abar_sz <= PG_SZ);

  // Number of implemented ports from host capability register and
  // from enumerating ports with "port implemented" set should be
  // equal.
  uint8_t num_ports = (abar->cap & 0x1F) + 1;
  assert(enumerate_ports(*abar) == num_ports);

  if (max_port != num_ports) {
    nonstd::printf(
        "warning: AHCI max_port=%u != num_ports=%u. This means some wasted"
        "space in the AHCI memory areas, which are allocated in an array\r\n",
        max_port, num_ports);
  }

  if (!rebase_port_memory(*abar, max_port)) {
    return false;
  }

#ifdef DEBUG
  nonstd::printf("ABAR info:\r\n"
                 "\tphys=0x%x virt (ioremap)=0x%x sz=0x%x\r\n"
                 "\tglobal hba control=0x%x\r\n"
                 "\tnum ports=%u\r\n",
                 abar_phys, (size_t)abar, abar_sz, ghc, num_ports);
#endif

  return true;
}

bool read_blocking(uint8_t port_idx, uint32_t startl, uint32_t starth,
                   uint32_t count, uint16_t *buf) {
  // Output buffer must be sector_aligned.
  assert(util::algorithm::aligned_pow2<512>((size_t)buf));

  assert(abar != nullptr);
  auto &port = abar->ports[port_idx];

  // Clear pending interrupt bits.
  uint32_t cleared_is = ~0U;
  objutil::copy_to_volatile(port.is,
                            reinterpret_cast<decltype(port.is) &>(cleared_is));
  int slot = find_free_cmdslot(port);
  if (unlikely(slot == -1)) {
    return false;
  }

  size_t cmdheader_phys = port.clb;
  auto *cmdheader =
      reinterpret_cast<CommandHeader *>(ahci_phys_to_virt(cmdheader_phys));

  cmdheader += slot;
  cmdheader->cfl =
      sizeof(fis::RegisterH2D) / sizeof(uint32_t); // Command FIS size
  cmdheader->w = 0;                                // Read from device
  unsigned prdt_entries_count = (uint16_t)((count - 1) >> 4) + 1;
  if (prdt_entries_count > prdtes_per_command) {
    return false;
  }
  cmdheader->prdtl = prdt_entries_count;

  size_t cmdtbl_phys = cmdheader->ctba;
  auto *cmdtbl =
      reinterpret_cast<CommandTable *>(ahci_phys_to_virt(cmdtbl_phys));
  nonstd::memset(cmdtbl, 0,
                 sizeof(CommandTable) + cmdheader->prdtl * sizeof(PRDTEntry));

  // 8K bytes (16 sectors) per PRDT entry.
  //
  // \note 8KB is chosen somewhat arbitrarily, copied from OSDev. It
  // can be up to 4MB at a time.
  //
  // \note For now, assume \a buf is a virtual address in the HHDM. If
  // this assumption ever breaks, hhdm_to_direct() will throw. (And
  // we'll need to either support different virt->phys addr
  // translations or have this function just take a phys addr).
  //
  // TODO: actually change this to 4MB chunks
  //
  int i;
  for (i = 0; i < cmdheader->prdtl - 1; i++) {
    cmdtbl->prdt_entry[i].dba = (uint32_t)mem::virt::hhdm_to_direct(buf);
    cmdtbl->prdt_entry[i].dbc =
        8 * 1024 - 1; // 8K bytes (this value should always be set to 1 less
                      // than the actual value)
    cmdtbl->prdt_entry[i].i = 1;
    buf += 4 * 1024; // 4K words
    count -= 16;     // 16 sectors
  }
  // Last entry
  cmdtbl->prdt_entry[i].dba = (uint32_t)mem::virt::hhdm_to_direct(buf);
  cmdtbl->prdt_entry[i].dbc = (count << 9) - 1; // 512 bytes per sector
  cmdtbl->prdt_entry[i].i = 1;

  // Setup command
  auto *cmdfis = reinterpret_cast<fis::RegisterH2D *>(&cmdtbl->cfis);

  cmdfis->fis_type = fis::Type::RegisterH2D;
  cmdfis->c = 1; // Command
  cmdfis->command = fis::ATACommand::ReadDMAExt;

  cmdfis->lba0 = (uint8_t)startl;
  cmdfis->lba1 = (uint8_t)(startl >> 8);
  cmdfis->lba2 = (uint8_t)(startl >> 16);
  cmdfis->device = 1 << 6; // LBA mode

  cmdfis->lba3 = (uint8_t)(startl >> 24);
  cmdfis->lba4 = (uint8_t)starth;
  cmdfis->lba5 = (uint8_t)(starth >> 8);

  cmdfis->countl = count & 0xFF;
  cmdfis->counth = (count >> 8) & 0xFF;

  // Wait until port is not busy before sending command -- at least 1M
  // cycles (>>1ms given that port reads are slow).
  unsigned spin = 0;
  while (bw::and_(port.tfd, decltype(port.tfd){.drq = 1, .bsy = 1})) {
    if (unlikely(++spin == 1'000'000)) {
      nonstd::printf("Port is hung\n");
      return false;
    }
  }

  port.ci = 1 << slot; // Issue command

  // Wait for completion
  const auto tfes_err = decltype(port.is){.tfes = 1};
  while (1) {
    // In some longer duration reads, it may be helpful to spin on the DPS bit
    // in the PxIS port field as well (1 << 5)
    if ((port.ci & (1 << slot)) == 0) {
      break;
    }

    if (bw::and_(port.is, tfes_err)) {
      // Task file error
      nonstd::printf("Read disk error 0\n");
      return false;
    }
  }

  // Check again
  if (bw::and_(port.is, tfes_err)) {
    nonstd::printf("Read disk error 1\n");
    return false;
  }

  return true;
}

} // namespace drivers::ahci
