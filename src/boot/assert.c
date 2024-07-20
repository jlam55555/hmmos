#include "console.h"

void __assert_fail(const char *assertion, const char *file, unsigned int line,
                   const char *function) {
  // We don't have access to printf in libc_minimal, so use the basic
  // console_*() functions.
  console_puts(file);
  console_printw(line); // Prints in hex; a little awkward.
  console_puts2(function, "(): assert(", assertion, ") failed\r\n");

  for (;;) {
    __asm__ volatile("hlt");
  }
}
