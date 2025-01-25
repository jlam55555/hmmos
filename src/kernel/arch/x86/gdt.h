#pragma once

/// \file gdt.h
/// \brief Global Descriptor Table (GDT) and Task State Segment (TSS)
///
/// \see idt.h for IDT

#include <array>
#include <cstddef>
#include <cstdint>

namespace arch::gdt {

/// An entry in the GDT
///
struct GDTSegment {
  uint16_t limit0;
  uint16_t base0;
  uint8_t base1;
  uint8_t access;
  uint8_t limit1 : 4;
  uint8_t flags : 4;
  uint8_t base2;

  explicit GDTSegment(uint32_t limit, uint32_t base, uint8_t _access,
                      uint8_t _flags)
      : limit0{(uint16_t)limit},                 //
        base0{(uint16_t)base},                   //
        base1{(uint8_t)(base >> 16)},            //
        access{_access},                         //
        limit1{(uint8_t)((limit >> 16) & 0x0F)}, //
        flags{_flags},                           //
        base2{(uint8_t)(base >> 24)}             //
  {}
} __attribute__((packed));
static_assert(sizeof(GDTSegment) == 8);

/// Descriptor loaded into the GDTR register
///
struct GDTDescriptor {
  uint16_t sz;
  uint32_t off;

  template <size_t N>
  explicit GDTDescriptor(const std::array<GDTSegment, N> &gdt)
      : sz{N * sizeof(GDTSegment) - 1}, off{(size_t)gdt.data()} {}
} __attribute__((packed));
static_assert(sizeof(GDTDescriptor) == 6);

/// Task State Segment
///
/// Used for software multitasking. We only care about esp0/ss0 which
/// is used to return to the kernel stack on a privilege-level change.
/// Other process state is stored on the stack rather than in the TSS.
struct TSS {
  uint16_t link;
  uint16_t rsv0;
  uint32_t esp0;
  uint16_t ss0;
  uint16_t rsv1;
  uint32_t esp1;
  uint16_t ss1;
  uint16_t rsv2;
  uint32_t esp2;
  uint16_t ss2;
  uint16_t rsv3;
  uint32_t cr3;
  uint32_t eip;
  uint32_t eflags;
  uint32_t eax;
  uint32_t ecx;
  uint32_t edx;
  uint32_t ebx;
  uint32_t esp;
  uint32_t ebp;
  uint32_t esi;
  uint32_t edi;
  uint16_t es;
  uint16_t rsv4;
  uint16_t cs;
  uint16_t rsv5;
  uint16_t ss;
  uint16_t rsv6;
  uint16_t ds;
  uint16_t rsv7;
  uint16_t fs;
  uint16_t rsv8;
  uint16_t gs;
  uint16_t rsv9;
  uint16_t ldtr;
  uint16_t rsv10;
  uint16_t rsv11;
  uint16_t iopb;
  uint32_t ssp;
};
static_assert(sizeof(TSS) == 0x6C);

/// Re-init the GDT.
///
/// We need to reconfigure this as the original GDT lies in
/// bootloader-reclaimable memory. This also includes the TSS and ring
/// 3 segments which were not present in the bootloader GDT (and also
/// doesn't include the unreal mode segment.
///
/// Memory segments:
///     GDT[0] = null descriptor
///     GDT[1] = ring 0 code
///     GDT[2] = ring 0 data
///     GDT[3] = ring 3 code
///     GDT[4] = ring 3 data
///     GDT[5] = TSS
///
void init();

/// Set esp0 upon a context switch.
///
void set_tss_esp0(void *esp0);

} // namespace arch::gdt
