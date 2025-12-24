#pragma once

/// \file
/// \brief Very simple printing utilities via BIOS text screen memory.
///
/// The bootloader uses these directly, while the kernel has more
/// advanced printf-like functions. In the interest of not including
/// the whole printf* functionality to the bootloader, this is the
/// only shared logic.
///
/// These technically should be usable from (un)real and protected
/// mode, but these are only currently used in protected mode. The
/// bootloader code contains very simple real-mode printing logic.

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/// \brief Write a single character to the console buffer.
///
void console_putchar(char c);

/// \brief Flush the console buffer to screen.
///
void console_flush();

/// \brief Write (and flush) a null-terminated string. \r and \n escape codes
/// are interpreted.
void console_puts(const char *str);

/// \brief Call console_puts on each of its string arguments.
///
/// (The first string argument is only present since we can't have
/// variadic functions with zero non-variadic arguments.)
void console_puts2(const char *s, ...);

// Similar to rmode_print{b,w,l}.
void console_printb(const uint8_t n);
void console_printw(const uint16_t n);
void console_printl(const uint32_t n);
void console_printq(const uint64_t n);

/// Used by the kernel; switch the VGA buf to the HHDM-mapped address
/// since userspace processes don't use the direct low memory map.
void console_use_hhdm();

#ifdef __cplusplus
}
#endif
