#include "console.h"
#include "libc_minimal.h"
#include "memdefs.h"
#include <stdarg.h>
#include <stdint.h>

#define CONSOLE_WIDTH 80
#define CONSOLE_HEIGHT 25

static uint8_t _console_x = 0;
static uint8_t _console_y = 0;
static char _console_buf[CONSOLE_WIDTH * CONSOLE_HEIGHT] = {};
static char *console_vga_buf = (char *)0xB8000;

/// Advance cursor to next line, scrolling if needed.
static void _console_cursor_down() {
  ++_console_y;
  if (_console_y >= CONSOLE_HEIGHT) {
    --_console_y;
    for (uint8_t y = 0; y < CONSOLE_HEIGHT - 1; ++y) {
      memcpy(&_console_buf[y * CONSOLE_WIDTH],
             &_console_buf[(y + 1) * CONSOLE_WIDTH], CONSOLE_WIDTH);
    }
    memset(&_console_buf[(CONSOLE_HEIGHT - 1) * CONSOLE_WIDTH], 0,
           CONSOLE_WIDTH);
  }
}

/// Advance cursor forward, wrapping if needed.
static void _console_cursor_forward() {
  ++_console_x;
  if (_console_x >= CONSOLE_WIDTH) {
    _console_cursor_down();
    _console_x = 0;
  }
}

void console_putchar(char c) {
  if (c == '\n') {
    _console_cursor_down();
  } else if (c == '\r') {
    _console_x = 0;
  } else {
    _console_buf[CONSOLE_WIDTH * _console_y + _console_x] = c;
    _console_cursor_forward();
  }
}

void console_flush() {
  for (unsigned i = 0; i < sizeof _console_buf; ++i) {
    console_vga_buf[i << 1] = _console_buf[i];
  }
}

void console_puts(const char *str) {
  while (*str) {
    console_putchar(*str++);
  }
  console_flush();
}

void console_puts2(const char *s, ...) {
  console_puts(s);

  const char *arg;
  va_list args;
  va_start(args, s);
  while ((arg = va_arg(args, const char *)) != NULL) {
    console_puts(arg);
  }
  va_end(args);
}

static char _console_int_buf[2 + 2 * 8 + 1] = "0x";
static char _console_nyb2hex(uint8_t nybble) {
  return nybble < 10 ? (nybble + '0') : (nybble + 'A' - 10);
}
static void _console_format_byte(uint8_t n, char *buf) {
  *buf = _console_nyb2hex(n >> 4);
  *(buf + 1) = _console_nyb2hex(n & 0x0F);
}
static void _console_printn(uint64_t n, unsigned sz) {
  n = __builtin_bswap64(n) >> (64 - 8 * sz);
  char *buf = _console_int_buf + 2;
  for (; sz--; n >>= 8, buf += 2) {
    _console_format_byte(n, buf);
  }
  *buf = '\0';
  console_puts(_console_int_buf);
}

void console_printb(uint8_t n) { _console_printn(n, 1); }
void console_printw(uint16_t n) { _console_printn(n, 2); }
void console_printl(uint32_t n) { _console_printn(n, 4); }
void console_printq(uint64_t n) { _console_printn(n, 8); }

void console_use_hhdm() {
  console_vga_buf = (char *)(HM_START | (size_t)console_vga_buf);
}
