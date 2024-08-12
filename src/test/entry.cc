#include "../crt/crt.h"
#include "drivers/acpi.h"
#include "drivers/serial.h"
#include "nonstd/libc.h"
#include "test.h"

__attribute__((section(".text.entry"))) void _entry() {
  crt::run_global_ctors();

  // TODO: could use scanf() logic
  char c;
  char test_selection_buf[4096] = {};
  unsigned pos = 0;

  while ((c = serial::get().read()) != '\n' &&
         pos < sizeof test_selection_buf) {
    test_selection_buf[pos++] = c;
  }

  test::run_tests(test_selection_buf);

  crt::run_global_dtors();
  acpi::shutdown();
}
