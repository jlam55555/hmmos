#include "pmode_print.h"
#include "string.h"
#include <stdint.h>

#define _screen_width 80
#define _screen_height 25

#define _vid_buf ((char *const)0xB8000)
#define _vid_buf_width (2 * _screen_width)

static uint8_t _screen_x = 0;
static uint8_t _screen_y = 0;
static char _screen_buf[_screen_width * _screen_height] = {};

/// Advance cursor to next line, scrolling if needed.
static void _screen_cursor_down() {
  ++_screen_y;
  if (_screen_y >= _screen_height) {
    --_screen_y;
    for (uint8_t y = 0; y < _screen_height - 1; ++y) {
      memcpy(&_screen_buf[y * _screen_width],
             &_screen_buf[(y + 1) * _screen_width], _screen_width);
    }
    memset(&_screen_buf[(_screen_height - 1) * _screen_width], 0,
           _screen_width);
  }
}

/// Advance cursor forward, wrapping if needed.
static void _screen_cursor_forward() {
  ++_screen_x;
  if (_screen_x >= _screen_width) {
    _screen_cursor_down();
    _screen_x = 0;
  }
}

static void _screen_putc(const char c) {
  if (c == '\n') {
    _screen_cursor_down();
  } else if (c == '\r') {
    _screen_x = 0;
  } else {
    _screen_buf[_screen_width * _screen_y + _screen_x] = c;
    _screen_cursor_forward();
  }
}

static void _screen_render() {
  for (unsigned i = 0; i < sizeof _screen_buf; ++i) {
    _vid_buf[i << 1] = _screen_buf[i];
  }
}

void pmode_puts(const char *str) {
  while (*str) {
    _screen_putc(*str++);
  }

  _screen_render();
}

static char _screen_int_buf[2 + 2 * 8 + 1] = "0x";

static char __screen_nyb2hex(uint8_t nybble) {
  return nybble < 10 ? (nybble + '0') : (nybble + 'A' - 10);
}
static void _screen_format_byte(uint8_t n, char *buf) {
  *buf = __screen_nyb2hex(n >> 4);
  *(buf + 1) = __screen_nyb2hex(n & 0x0F);
}

static void _screen_printn(uint64_t n, unsigned sz) {
  n = __builtin_bswap64(n) >> (64 - 8 * sz);
  char *buf = _screen_int_buf + 2;
  for (; sz--; n >>= 8, buf += 2) {
    _screen_format_byte(n, buf);
  }
  *buf = '\0';
  pmode_puts(_screen_int_buf);
}

void pmode_printb(uint8_t n) { _screen_printn(n, 1); }
void pmode_printw(uint16_t n) { _screen_printn(n, 2); }
void pmode_printl(uint32_t n) { _screen_printn(n, 4); }
void pmode_printq(uint64_t n) { _screen_printn(n, 8); }
