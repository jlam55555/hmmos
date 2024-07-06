#pragma once

/// Very simple printing utilities via BIOS text screen memory. Can
/// be used in protected mode or unreal mode (in which the BIOS video
/// buffer is accessible).
///
/// This same logic can be reused by the kernel.
#include <stdint.h>

/// Write a null-terminated string. \n and \r escape codes are
/// interpreted.
void pmode_puts(const char *str);

/// Similar to real-mode printb, printw, printl.
void pmode_printb(const uint8_t n);
void pmode_printw(const uint16_t n);
void pmode_printl(const uint32_t n);
void pmode_printq(const uint64_t n);
