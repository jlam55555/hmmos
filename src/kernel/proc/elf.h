#pragma once

/// \file elf.h
/// \brief ELF file parser

#include "fs/result.h"
#include "fs/vfs.h"
#include "nonstd/string_view.h"
#include "nonstd/vector.h"

namespace proc::elf {

/// File header.
struct ELFHeader {
  // 0x7F+"ELF"
  static constexpr uint32_t expected_magic = 0x46'4c'45'7F;

  static constexpr uint8_t expected_class = 0x01;      // 32-bit
  static constexpr uint8_t expected_endian = 0x01;     // Little-endian
  static constexpr uint8_t expected_version = 0x01;    // ELF version
  static constexpr uint8_t expected_os_abi = 0x00;     // System V
  static constexpr uint8_t expected_type = 0x02;       // Executable
  static constexpr uint8_t expected_machine = 0x03;    // x86
  static constexpr uint16_t expected_ehsize = 0x34;    // Header size
  static constexpr uint16_t expected_phentsize = 0x20; // PH entry size
  static constexpr uint16_t expected_shentsize = 0x28; // SH entry size

  uint32_t magic;
  uint8_t class_;
  uint8_t endian;
  uint8_t version;
  uint8_t os_abi;
  uint8_t abi_version;
  uint64_t rsv0 : 56;
  uint16_t type;
  uint16_t machine;
  uint32_t version2;
  uint32_t entry;
  uint32_t phoff;
  uint32_t shoff;
  uint32_t flags;
  uint16_t ehsize;
  uint16_t phentsize;
  uint16_t phnum;
  uint16_t shentsize;
  uint16_t shnum;
  uint16_t shstrndx;

  /// Returns whether this is an executable file that we know how to
  /// execute.
  bool validate() const;
};
static_assert(sizeof(ELFHeader) == ELFHeader::expected_ehsize);

// Program header entry
struct PHEntry {
  enum class Type : uint32_t {
    Null = 0x00,
    Load = 0x01,
    Dynamic = 0x02,
    Interp = 0x03,
    Note = 0x04,
    SharedLibrary = 0x05,
    ProgramHeader = 0x06,
    ThreadLocalStorage = 0x07,
    LoOSSpecific = 0x60000000,
    HiOSSpecific = 0x6FFFFFFF,
    LoProcSpecific = 0x70000000,
    HiProcSpecific = 0x7FFFFFFF,
  };

  static constexpr uint8_t flag_executable = 0x01;
  static constexpr uint8_t flag_writable = 0x02;
  static constexpr uint8_t flag_readable = 0x04;

  Type type;
  uint32_t offset;
  uint32_t vaddr;
  uint32_t paddr;
  uint32_t filesz;
  uint32_t memsz;
  uint32_t flags;
  uint32_t align;

  bool executable() const { return flags & flag_executable; }
  bool writable() const { return flags & flag_writable; }
  bool readable() const { return flags & flag_readable; }
};
static_assert(sizeof(PHEntry) == ELFHeader::expected_phentsize);

/// Parsed ELF executable image.
struct ELFExecutable {
  const ELFHeader *hdr;
  const PHEntry *ph_table;
};

/// Parse an ELF file, using some prefix of the ELF image.
fs::Result parse_executable(nonstd::string_view elf_image,
                            ELFExecutable &parsed_image);

} // namespace proc::elf
