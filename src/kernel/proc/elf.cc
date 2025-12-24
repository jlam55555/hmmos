#include "proc/elf.h"

namespace proc::elf {

bool ELFHeader::validate() const {
  return magic == expected_magic &&         //
         class_ == expected_class &&        //
         endian == expected_endian &&       //
         version == expected_version &&     //
         os_abi == expected_os_abi &&       //
         type == expected_type &&           //
         machine == expected_machine &&     //
         ehsize == expected_ehsize &&       //
         phentsize == expected_phentsize && //
         shentsize == expected_shentsize && //
         entry != 0;
}

fs::Result parse_executable(nonstd::string_view elf_image,
                            ELFExecutable &parsed_image) {
  // This can technically happen. But we always load one 4KB page, and
  // expect the program header to fit within this space. If this
  // assumption changes, we need to update the exec() code.
  ASSERT(elf_image.length() >= ELFHeader::expected_ehsize);

  auto *hdr = parsed_image.hdr =
      reinterpret_cast<const ELFHeader *>(elf_image.data());
  if (!hdr->validate()) {
    return fs::Result::NonExecutable;
  }

  // See note above.
  ASSERT(elf_image.length() >= hdr->phoff + hdr->phnum * hdr->phentsize);

  parsed_image.ph_table =
      reinterpret_cast<const PHEntry *>(elf_image.data() + hdr->phoff);
  return fs::Result::Ok;
}

} // namespace proc::elf
