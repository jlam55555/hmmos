#include "pmode_print.h"

// Required for <assert.h>
void __assert_fail(const char *assertion, const char *file, unsigned int line,
                   const char *function) {
  // TODO: man I really need a printf function
  pmode_puts(file);
  pmode_puts(":");
  pmode_printw(line);
  pmode_puts(" (in ");
  pmode_puts(function);
  pmode_puts("): assert(");
  pmode_puts(assertion);
  pmode_puts(") failed\r\n");

  for (;;) {
    __asm__ volatile("hlt");
  }
}
