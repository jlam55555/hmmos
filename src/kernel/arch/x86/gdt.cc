#include "gdt.h"
#include "util/bw.h"
#include <array>

namespace arch::gdt {

namespace {

/// Structure of the access byte for code/data segments
///
struct Access {
  uint8_t a : 1;
  uint8_t rw : 1;
  uint8_t dc : 1;
  uint8_t e : 1;
  uint8_t s : 1;
  uint8_t dpl : 2;
  uint8_t p : 1;
};
static_assert(sizeof(Access) == 1);

/// Structure of the access byte for system segments (e.g. TSS)
///
struct SystemAccess {
  uint8_t type : 4;
  uint8_t s : 1;
  uint8_t dpl : 2;
  uint8_t p : 1;
};
static_assert(sizeof(SystemAccess) == 1);

/// Format of the flags nybble
///
struct Flags {
  uint8_t rsv : 1 = 0;
  uint8_t l : 1;
  uint8_t db : 1;
  uint8_t g : 1;
};

uint8_t generate_access(bool is_code, uint8_t dpl) {
  return bw::id(Access{
      .a = 1,
      .rw = 1,
      .dc = 0,
      .e = is_code,
      .s = 1,
      .dpl = dpl,
      .p = 1,
  });
}

const uint32_t max_limit = 0x0000FFFFF;
const uint8_t flags = bw::id(Flags{
    .l = 0,
    .db = 1,
    .g = 1,
});

TSS tss{
    // This will be set in \ref set_tss_esp0().
    .esp0 = 0,
    // GDT data segment
    .ss0 = 0x10,
    // No IO bitmap
    .iopb = sizeof(TSS),
};

const std::array gdt{
    // Null descriptor,
    bw::not_<GDTSegment>(~0),

    // Ring 0 code + data (these must match the segments set up by the
    // bootloader since the segments are already loaded).
    GDTSegment{max_limit, 0, generate_access(true, 0), flags},
    GDTSegment{max_limit, 0, generate_access(false, 0), flags},

    // Ring 3 code + data
    GDTSegment{max_limit, 0, generate_access(true, 3), flags},
    GDTSegment{max_limit, 0, generate_access(false, 3), flags},

    // TSS
    GDTSegment{sizeof(TSS), (size_t)&tss,
               bw::id(SystemAccess{
                   .type = 0x09,
                   .s = 0,
                   .dpl = 0,
                   .p = 1,
               }),
               bw::id(Flags{
                   .l = 0,
                   .db = 0,
                   .g = 0,
               })},
};

const GDTDescriptor gdt_desc{gdt};

} // namespace

void init() {
  const uint16_t tss_desc = (char *)&gdt[5] - (char *)&gdt;
  __asm__("lgdt %0\n\t"
          "ltr %1"
          :
          : "m"(gdt_desc), "m"(tss_desc));
}

void set_tss_esp0(void *esp0) { tss.esp0 = (size_t)esp0; }

} // namespace arch::gdt
