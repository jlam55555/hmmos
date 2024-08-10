#include "isrs.h"

#include "drivers/acpi.h"
#include "drivers/pic.h"
#include "nonstd/libc.h"

extern void (*__start_text_isrs)();
extern void (*__stop_text_isrs)();

void (**isrs)() = &__start_text_isrs;
unsigned num_isrs() { return &__stop_text_isrs - &__start_text_isrs; }

extern "C" {

__attribute__((noreturn)) void isr_dumpregs_errcode(uint32_t ivec,
                                                    RegisterFrame reg_frame,
                                                    uint32_t error_code,
                                                    InterruptFrame frame) {
  // TODO: print other useful registers like CR*
  nonstd::printf(
      "INT 0x%x err=0x%x eip=0x%x cs=0x%x eflags=0x%x\r\nedi=0x%x esi=0x%x "
      "ebp=0x%x esp=0x%x ebx=0x%x edx=0x%x ecx=0x%x eax=0x%x\r\n",
      ivec, error_code, frame.eip, frame.cs, frame.eflags, reg_frame.edi,
      reg_frame.esi, reg_frame.ebp, reg_frame.esp + sizeof frame, reg_frame.ebx,
      reg_frame.edx, reg_frame.ecx, reg_frame.eax);
  acpi::shutdown();
}

__attribute__((noreturn)) void
isr_dumpregs(uint32_t ivec, RegisterFrame reg_frame, InterruptFrame frame) {
  // This pushes a bunch of stuff onto the stack, but this is okay
  // since we're about to shutdown anyways. It's only not okay if
  // we're out of (kernel) stack space.
  isr_dumpregs_errcode(ivec, reg_frame, 0, frame);
}

void isr_pit(uint32_t ivec, RegisterFrame reg_frame, InterruptFrame frame) {
  // Nothing to do here ... yet. Until we have pre-emptive scheduling.
  drivers::pic::eoi(0);
}
}

// Helper function; don't use directly.
//
// Not using __attribute__((interrupt)) since it's not easy to read
// register values from there.
#define _ISR(IVEC, C_ENTRY, POP_ERR)                                           \
  __attribute__((naked)) void isr_##IVEC() {                                   \
    __asm__ volatile("pusha\n\t"                                               \
                     "push $" #IVEC "\n\t"                                     \
                     "call " #C_ENTRY "\n\t"                                   \
                     "pop %eax\n\t"                                            \
                     "popa\n\t" POP_ERR "\n\t"                                 \
                     "iret");                                                  \
  }                                                                            \
  __attribute__((section("text_isrs"))) void (*_isr_##IVEC)() = &isr_##IVEC;

/// Use ISR to declare an ISR for an interrupt or an exception that
/// doesn't push an error code onto the stack. Use ISRE for exceptions
/// that do push an error code onto the stack (and use
/// `isr_dumpregs_errcode()`.
#define ISR(IVEC, C_ENTRY) _ISR(IVEC, C_ENTRY, "")
#define ISRE(IVEC, C_ENTRY) _ISR(IVEC, C_ENTRY, "add $4, %esp")

/// Order matters here! We are basically writing these directly to an
/// array (in a special text section). ISRs 0-31 are reserved for
/// exceptions, and 32-255 are for (maskable) interrupts. The #XX
/// abbreviations are mnemonics used by Intel.
///
/// #DF, #TS, #NP, #SS, #GP, #PF, #AC, #XM, #CP push error codes onto
/// #the stack, and these need to be popped correctly using ISRE().
///
/// For now the default action for unhandled interrupts to dump
/// registers and shutdown. I couldn't think of a smarter way to do
/// this :/

/// (x86) Exceptions
ISR(0x00, isr_dumpregs);          // #DE Division error
ISR(0x01, isr_dumpregs);          // #DB Debug
ISR(0x02, isr_dumpregs);          // NMI
ISR(0x03, isr_dumpregs);          // #BP Breakpoint
ISR(0x04, isr_dumpregs);          // #OF Overflow
ISR(0x05, isr_dumpregs);          // #BR Bound range exceeded
ISR(0x06, isr_dumpregs);          // #UD Invalid opode
ISRE(0x07, isr_dumpregs_errcode); // #DF Device not available
ISR(0x08, isr_dumpregs);          // #DF Double fault
ISR(0x09, isr_dumpregs);          // #MF Coprocessor segment overrun
ISRE(0x0A, isr_dumpregs_errcode); // #TS Invalid TSS
ISRE(0x0B, isr_dumpregs_errcode); // #NP Segment not present
ISRE(0x0C, isr_dumpregs_errcode); // #SS Stack-segment fault
ISRE(0x0D, isr_dumpregs_errcode); // #GP General protection fault
ISRE(0x0E, isr_dumpregs_errcode); // #PF Page fault
ISR(0x0F, isr_dumpregs);          // reserved
ISR(0x10, isr_dumpregs);          // #MF x87 floating-point exception
ISRE(0x11, isr_dumpregs_errcode); // #AC Alignment check
ISR(0x12, isr_dumpregs);          // #MC Machine check
ISRE(0x13, isr_dumpregs_errcode); // #XM SIMD floating-point exception
ISR(0x14, isr_dumpregs);          // #VE Virtualization exception
ISRE(0x15, isr_dumpregs_errcode); // #CP Control protection exception
ISR(0x16, isr_dumpregs);          // reserved
ISR(0x17, isr_dumpregs);          // reserved
ISR(0x18, isr_dumpregs);          // reserved
ISR(0x19, isr_dumpregs);          // reserved
ISR(0x1A, isr_dumpregs);          // reserved
ISR(0x1B, isr_dumpregs);          // reserved
ISR(0x1C, isr_dumpregs);          // reserved
ISR(0x1D, isr_dumpregs);          // reserved
ISR(0x1E, isr_dumpregs);          // reserved
ISR(0x1F, isr_dumpregs);          // reserved

/// Interrupts. IRQs 0-15 are standard ISA interrupts and will be
/// remapped to the first 15 interrupt here via the PIC.
ISR(0x20, isr_pit);      // IRQ0: PIT
ISR(0x21, isr_dumpregs); // IRQ1: Keyboard
ISR(0x22, isr_dumpregs); // Cascade interrupt (used internally by PIC)
ISR(0x23, isr_dumpregs); // IRQ3: COM2
ISR(0x24, isr_dumpregs); // IRQ4: COM1
ISR(0x25, isr_dumpregs); // IRQ5: LPT2
ISR(0x26, isr_dumpregs); // IRQ6: Floppy disk
ISR(0x27, isr_dumpregs); // IRQ7: LPT1
ISR(0x28, isr_dumpregs); // IRQ8: CMOS
ISR(0x29, isr_dumpregs); // IRQ9: free
ISR(0x2A, isr_dumpregs); // IRQ10: free
ISR(0x2B, isr_dumpregs); // IRQ11: free
ISR(0x2C, isr_dumpregs); // IRQ12: PS2 mouse
ISR(0x2D, isr_dumpregs); // IRQ13: FPU / coprocessor
ISR(0x2E, isr_dumpregs); // IRQ14: Primary ATA hard disk
ISR(0x2F, isr_dumpregs); // IRQ15: Secondary ATA hard disk

ISR(0x30, isr_dumpregs);
ISR(0x31, isr_dumpregs);
ISR(0x32, isr_dumpregs);
ISR(0x33, isr_dumpregs);
ISR(0x34, isr_dumpregs);
ISR(0x35, isr_dumpregs);
ISR(0x36, isr_dumpregs);
ISR(0x37, isr_dumpregs);
ISR(0x38, isr_dumpregs);
ISR(0x39, isr_dumpregs);
ISR(0x3A, isr_dumpregs);
ISR(0x3B, isr_dumpregs);
ISR(0x3C, isr_dumpregs);
ISR(0x3D, isr_dumpregs);
ISR(0x3E, isr_dumpregs);
ISR(0x3F, isr_dumpregs);
ISR(0x40, isr_dumpregs);
ISR(0x41, isr_dumpregs);
ISR(0x42, isr_dumpregs);
ISR(0x43, isr_dumpregs);
ISR(0x44, isr_dumpregs);
ISR(0x45, isr_dumpregs);
ISR(0x46, isr_dumpregs);
ISR(0x47, isr_dumpregs);
ISR(0x48, isr_dumpregs);
ISR(0x49, isr_dumpregs);
ISR(0x4A, isr_dumpregs);
ISR(0x4B, isr_dumpregs);
ISR(0x4C, isr_dumpregs);
ISR(0x4D, isr_dumpregs);
ISR(0x4E, isr_dumpregs);
ISR(0x4F, isr_dumpregs);
ISR(0x50, isr_dumpregs);
ISR(0x51, isr_dumpregs);
ISR(0x52, isr_dumpregs);
ISR(0x53, isr_dumpregs);
ISR(0x54, isr_dumpregs);
ISR(0x55, isr_dumpregs);
ISR(0x56, isr_dumpregs);
ISR(0x57, isr_dumpregs);
ISR(0x58, isr_dumpregs);
ISR(0x59, isr_dumpregs);
ISR(0x5A, isr_dumpregs);
ISR(0x5B, isr_dumpregs);
ISR(0x5C, isr_dumpregs);
ISR(0x5D, isr_dumpregs);
ISR(0x5E, isr_dumpregs);
ISR(0x5F, isr_dumpregs);
ISR(0x60, isr_dumpregs);
ISR(0x61, isr_dumpregs);
ISR(0x62, isr_dumpregs);
ISR(0x63, isr_dumpregs);
ISR(0x64, isr_dumpregs);
ISR(0x65, isr_dumpregs);
ISR(0x66, isr_dumpregs);
ISR(0x67, isr_dumpregs);
ISR(0x68, isr_dumpregs);
ISR(0x69, isr_dumpregs);
ISR(0x6A, isr_dumpregs);
ISR(0x6B, isr_dumpregs);
ISR(0x6C, isr_dumpregs);
ISR(0x6D, isr_dumpregs);
ISR(0x6E, isr_dumpregs);
ISR(0x6F, isr_dumpregs);
ISR(0x70, isr_dumpregs);
ISR(0x71, isr_dumpregs);
ISR(0x72, isr_dumpregs);
ISR(0x73, isr_dumpregs);
ISR(0x74, isr_dumpregs);
ISR(0x75, isr_dumpregs);
ISR(0x76, isr_dumpregs);
ISR(0x77, isr_dumpregs);
ISR(0x78, isr_dumpregs);
ISR(0x79, isr_dumpregs);
ISR(0x7A, isr_dumpregs);
ISR(0x7B, isr_dumpregs);
ISR(0x7C, isr_dumpregs);
ISR(0x7D, isr_dumpregs);
ISR(0x7E, isr_dumpregs);
ISR(0x7F, isr_dumpregs);
ISR(0x80, isr_dumpregs); // x86 syscall (SW interrupt)
ISR(0x81, isr_dumpregs);
ISR(0x82, isr_dumpregs);
ISR(0x83, isr_dumpregs);
ISR(0x84, isr_dumpregs);
ISR(0x85, isr_dumpregs);
ISR(0x86, isr_dumpregs);
ISR(0x87, isr_dumpregs);
ISR(0x88, isr_dumpregs);
ISR(0x89, isr_dumpregs);
ISR(0x8A, isr_dumpregs);
ISR(0x8B, isr_dumpregs);
ISR(0x8C, isr_dumpregs);
ISR(0x8D, isr_dumpregs);
ISR(0x8E, isr_dumpregs);
ISR(0x8F, isr_dumpregs);
ISR(0x90, isr_dumpregs);
ISR(0x91, isr_dumpregs);
ISR(0x92, isr_dumpregs);
ISR(0x93, isr_dumpregs);
ISR(0x94, isr_dumpregs);
ISR(0x95, isr_dumpregs);
ISR(0x96, isr_dumpregs);
ISR(0x97, isr_dumpregs);
ISR(0x98, isr_dumpregs);
ISR(0x99, isr_dumpregs);
ISR(0x9A, isr_dumpregs);
ISR(0x9B, isr_dumpregs);
ISR(0x9C, isr_dumpregs);
ISR(0x9D, isr_dumpregs);
ISR(0x9E, isr_dumpregs);
ISR(0x9F, isr_dumpregs);
ISR(0xA0, isr_dumpregs);
ISR(0xA1, isr_dumpregs);
ISR(0xA2, isr_dumpregs);
ISR(0xA3, isr_dumpregs);
ISR(0xA4, isr_dumpregs);
ISR(0xA5, isr_dumpregs);
ISR(0xA6, isr_dumpregs);
ISR(0xA7, isr_dumpregs);
ISR(0xA8, isr_dumpregs);
ISR(0xA9, isr_dumpregs);
ISR(0xAA, isr_dumpregs);
ISR(0xAB, isr_dumpregs);
ISR(0xAC, isr_dumpregs);
ISR(0xAD, isr_dumpregs);
ISR(0xAE, isr_dumpregs);
ISR(0xAF, isr_dumpregs);
ISR(0xB0, isr_dumpregs);
ISR(0xB1, isr_dumpregs);
ISR(0xB2, isr_dumpregs);
ISR(0xB3, isr_dumpregs);
ISR(0xB4, isr_dumpregs);
ISR(0xB5, isr_dumpregs);
ISR(0xB6, isr_dumpregs);
ISR(0xB7, isr_dumpregs);
ISR(0xB8, isr_dumpregs);
ISR(0xB9, isr_dumpregs);
ISR(0xBA, isr_dumpregs);
ISR(0xBB, isr_dumpregs);
ISR(0xBC, isr_dumpregs);
ISR(0xBD, isr_dumpregs);
ISR(0xBE, isr_dumpregs);
ISR(0xBF, isr_dumpregs);
ISR(0xC0, isr_dumpregs);
ISR(0xC1, isr_dumpregs);
ISR(0xC2, isr_dumpregs);
ISR(0xC3, isr_dumpregs);
ISR(0xC4, isr_dumpregs);
ISR(0xC5, isr_dumpregs);
ISR(0xC6, isr_dumpregs);
ISR(0xC7, isr_dumpregs);
ISR(0xC8, isr_dumpregs);
ISR(0xC9, isr_dumpregs);
ISR(0xCA, isr_dumpregs);
ISR(0xCB, isr_dumpregs);
ISR(0xCC, isr_dumpregs);
ISR(0xCD, isr_dumpregs);
ISR(0xCE, isr_dumpregs);
ISR(0xCF, isr_dumpregs);
ISR(0xD0, isr_dumpregs);
ISR(0xD1, isr_dumpregs);
ISR(0xD2, isr_dumpregs);
ISR(0xD3, isr_dumpregs);
ISR(0xD4, isr_dumpregs);
ISR(0xD5, isr_dumpregs);
ISR(0xD6, isr_dumpregs);
ISR(0xD7, isr_dumpregs);
ISR(0xD8, isr_dumpregs);
ISR(0xD9, isr_dumpregs);
ISR(0xDA, isr_dumpregs);
ISR(0xDB, isr_dumpregs);
ISR(0xDC, isr_dumpregs);
ISR(0xDD, isr_dumpregs);
ISR(0xDE, isr_dumpregs);
ISR(0xDF, isr_dumpregs);
ISR(0xE0, isr_dumpregs);
ISR(0xE1, isr_dumpregs);
ISR(0xE2, isr_dumpregs);
ISR(0xE3, isr_dumpregs);
ISR(0xE4, isr_dumpregs);
ISR(0xE5, isr_dumpregs);
ISR(0xE6, isr_dumpregs);
ISR(0xE7, isr_dumpregs);
ISR(0xE8, isr_dumpregs);
ISR(0xE9, isr_dumpregs);
ISR(0xEA, isr_dumpregs);
ISR(0xEB, isr_dumpregs);
ISR(0xEC, isr_dumpregs);
ISR(0xED, isr_dumpregs);
ISR(0xEE, isr_dumpregs);
ISR(0xEF, isr_dumpregs);
ISR(0xF0, isr_dumpregs);
ISR(0xF1, isr_dumpregs);
ISR(0xF2, isr_dumpregs);
ISR(0xF3, isr_dumpregs);
ISR(0xF4, isr_dumpregs);
ISR(0xF5, isr_dumpregs);
ISR(0xF6, isr_dumpregs);
ISR(0xF7, isr_dumpregs);
ISR(0xF8, isr_dumpregs);
ISR(0xF9, isr_dumpregs);
ISR(0xFA, isr_dumpregs);
ISR(0xFB, isr_dumpregs);
ISR(0xFC, isr_dumpregs);
ISR(0xFD, isr_dumpregs);
ISR(0xFE, isr_dumpregs);
ISR(0xFF, isr_dumpregs);

#undef ISR
