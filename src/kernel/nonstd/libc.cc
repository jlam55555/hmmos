#include "libc.h"
#include "console.h"
#include "drivers/acpi.h"
#include "drivers/serial.h"

namespace nonstd {

/// Since there are many definitions in this file, let's keep them in
/// the same order as the declarations in the header file for some
/// sense of sanity.

////////////////////////////////////////////////////////////////////////////////
// string.h
////////////////////////////////////////////////////////////////////////////////
__attribute__((nothrow)) int isprint(int c) { return c >= 32; }

////////////////////////////////////////////////////////////////////////////////
// string.h
////////////////////////////////////////////////////////////////////////////////

// nothing here for now; the string functions I need are all constexpr
// and thus inline (and thus in the header).

////////////////////////////////////////////////////////////////////////////////
// stdio.h
////////////////////////////////////////////////////////////////////////////////
enum format_spec_type {
  FS_SIGNED,
  FS_UNSIGNED,
  FS_HEX,
  FS_BINARY,
  FS_OCTAL,
  FS_STR,
  FS_CHAR,
  FS_PCT,
  FS_INVAL,
};

/// Currently, we assume the following integer
/// length specifiers:
/// - FS_NORMAL: 4 bytes
/// - FS_LONG: 8 bytes
/// - FS_LONG_LONG: (also) 8 bytes
/// - FS_SHORT: 2 bytes
/// - FS_SHORT_SHORT: 1 byte
///
/// Note: parse_format() depends on the order of this enum.
enum format_spec_length {
  FS_NORMAL,
  FS_LONG,
  FS_LONG_LONG,
  FS_SHORT,
  FS_SHORT_SHORT,
};

struct format_spec {
  enum format_spec_length length;
  enum format_spec_type type;
};

/// Parses a format specifier and returns the length of the specifier.
/// Only accepts a small number of length and type fields:
///
/// Integral types (also support l, ll, h, hh length specifiers):
/// - %d: signed int
/// - %u: unsigned int
/// - %b: binary (non-standard)
/// - %o: octal
/// - %x: hex
///
/// Other type specifiers:
/// - %%: literal '%'
/// - %c: character
/// - %s: null-terminated string
///
/// Note: it is UB to have an invalid format specifier, e.g.,
/// % not followed by a valid length/type specifier, or both
/// h and l specified.
static size_t _parse_format_spec(const char *s, struct format_spec *fs) {
  const char *it = s;
  if (*it != '%' || !*(it + 1)) {
    fs->type = FS_INVAL;
    return 1;
  }

  // Discard the initial % sign.
  ++it;

  // Get length specifier.
  fs->length = FS_NORMAL;
  if (*it == 'l') {
    while (fs->length < FS_LONG_LONG && *it == 'l') {
      ++reinterpret_cast<int &>(fs->length);
      ++it;
    }
  } else if (*it == 'h') {
    fs->length = FS_SHORT;
    while (fs->length < FS_SHORT_SHORT && *it == 'h') {
      ++reinterpret_cast<int &>(fs->length);
      ++it;
    }
  }

  // Get type specifier.
  switch (*it++) {
  case '%':
    fs->type = FS_PCT;
    break;
  case 'c':
    fs->type = FS_CHAR;
    break;
  case 'd':
    fs->type = FS_SIGNED;
    break;
  case 'u':
    fs->type = FS_UNSIGNED;
    break;
  case 'x':
    fs->type = FS_HEX;
    break;
  case 'o':
    fs->type = FS_OCTAL;
    break;
  case 'b':
    fs->type = FS_BINARY;
    break;
  case 's':
    fs->type = FS_STR;
    break;
  default:
    fs->type = FS_INVAL;
    return 1;
  }
  return it - s;
}

/// Used for the %b, %o, %u, %x format type specifiers.
/// Does not null-terminate. Assumes buf is large enough
/// to hold the result (at least 64 bytes).
static size_t _utoa(char *buf, uint64_t val, int radix) {
  int i = 0, j, tmp;
  char digit;

  // Edge case. Without this val=0 would give an empty string.
  if (!val) {
    buf[0] = '0';
    return 1;
  }

  switch (radix) {
  case 2:
  case 8:
  case 10:
  case 16:
    while (val) {
      digit = val % radix;
      buf[i++] = digit + (digit >= 10 ? 'a' - 10 : '0');
      val /= radix;
    }
    break;
  default:
    return -1;
  }
  // Reverse the digits.
  for (j = i / 2 - 1; j >= 0; --j) {
    tmp = buf[j];
    buf[j] = buf[i - 1 - j];
    buf[i - 1 - j] = tmp;
  }
  return i;
}

/// Used for the %d format type specifier. Doesn't null-terminate.
/// Assumes `buf` is large enough to hold the result.
size_t _itoa(char *buf, int64_t val) {
  // This depends on the fact that -(unsigned)INT_MIN ==
  // (unsigned)INT_MIN. So even though -INT_MIN would normally be UB
  // (integer overflow), that's not the case for unsigned ints.
  if (val < 0) {
    *buf++ = '-';
    return 1 + _utoa(buf, -(uint64_t)val, 10);
  } else {
    return _utoa(buf, val, 10);
  }
}

/// Interface for writer functions. Two current implementors:
/// - _buf_writer: write character to a buffer. Null-terminate.
/// - _term_writer: write character to a terminal-like object. Do not
///     null-terminate.
///
/// \param c Character to write.
/// \param buf Buffer to write to, if applicable
/// \param i Position in buffer to write to, if applicable
/// \param buf_sz Size of buffer to write to, if applicable
/// \param null_term If this is a null-terminator added by the *printf
///     logic rather than a null character inherent in the formatted
///     string. We want to keep track of this since it's possible to have
///     valid null characters within a (non-C-style) string buffer.
typedef void (*writer_t)(char c, char *buf, size_t i, size_t buf_sz,
                         bool null_term);

static inline void _buf_writer(char c, char *buf, size_t i, size_t buf_sz,
                               __attribute__((unused)) bool null_term) {
  if (i >= buf_sz) {
    return;
  }
  buf[i] = c;
}

static inline void _term_writer(char c, __attribute__((unused)) char *_buf,
                                __attribute__((unused)) size_t _i,
                                __attribute__((unused)) size_t _buf_sz,
                                bool null_term) {
  if (null_term) {
    return;
  }
  console_putchar(c);
  serial::get().write(c);
}

/// This returns the number of characters that would be printed
/// (excluding the null termination character) if n is sufficiently
/// large. I.e., if the return value is >= n, that means some
/// letter were truncated.
///
/// This is an internal implementation with inspiration from
/// https://github.com/mpaland/printf
///
/// TODO: implement flags, width, .precision, modifiers
static int _vsnprintf(writer_t write, char *buf, size_t n, const char *fmt,
                      va_list va) {
  size_t i, j, k, len, radix;
  int64_t d;
  uint64_t u;
  char c, scratch[64], *s;
  struct format_spec fs;

  for (i = 0, j = 0; fmt[i];) {
    // Format specifier.
    if (fmt[i] == '%') {
      i += _parse_format_spec(fmt + i, &fs);
      // Handle format specifier.
      switch (fs.type) {
      case FS_PCT:
        write('%', buf, j++, n, false);
        break;
      case FS_CHAR:
        // Note: char is promoted to int if passed as a vararg;
        // the program will abort if char is used.
        c = va_arg(va, int);
        write(c, buf, j++, n, false);
        break;
      case FS_BINARY:
      case FS_OCTAL:
      case FS_UNSIGNED:
      case FS_HEX:
        switch (fs.length) {
        case FS_NORMAL:
        case FS_SHORT:
        case FS_SHORT_SHORT:
          u = va_arg(va, uint32_t);
          break;
        case FS_LONG:
        case FS_LONG_LONG:
          u = va_arg(va, uint64_t);
          break;
        }
        switch (fs.type) {
        case FS_BINARY:
          radix = 2;
          break;
        case FS_OCTAL:
          radix = 8;
          break;
        case FS_UNSIGNED:
          radix = 10;
          break;
        default:
        case FS_HEX:
          radix = 16;
          break;
        }
        len = _utoa(scratch, u, radix);
        for (k = 0; k < len; ++k) {
          write(scratch[k], buf, j++, n, false);
        }
        break;
      case FS_SIGNED:
        switch (fs.length) {
        case FS_NORMAL:
        case FS_SHORT:
        case FS_SHORT_SHORT:
          d = va_arg(va, int32_t);
          break;
        case FS_LONG:
        case FS_LONG_LONG:
          d = va_arg(va, int64_t);
          break;
        }
        len = _itoa(scratch, d);
        for (k = 0; k < len; ++k) {
          write(scratch[k], buf, j++, n, false);
        }
        break;
      case FS_STR:
        for (s = va_arg(va, char *); *s; ++s) {
          write(*s, buf, j++, n, false);
        }
        break;
      case FS_INVAL:
        break;
      }
    }
    // Not a format specifier.
    else {
      write(fmt[i++], buf, j++, n, false);
    }
  }

  // Null-terminate.
  write('\0', buf, j < n ? j : n - 1, n, true);
  return j;
}

__attribute__((nothrow)) int vsnprintf(char *s, size_t n, const char *fmt,
                                       va_list va) {
  return _vsnprintf(_buf_writer, s, n, fmt, va);
}

__attribute__((nothrow)) int snprintf(char *s, size_t n, const char *fmt, ...) {
  va_list va;
  va_start(va, fmt);
  size_t rv = vsnprintf(s, n, fmt, va);
  va_end(va);
  return rv;
}

__attribute__((nothrow)) int vsprintf(char *s, const char *fmt, va_list va) {
  return vsnprintf(s, (size_t)-1, fmt, va);
}

__attribute__((nothrow)) int sprintf(char *s, const char *fmt, ...) {
  va_list va;
  va_start(va, fmt);
  size_t rv = vsprintf(s, fmt, va);
  va_end(va);
  return rv;
}

int vprintf(const char *fmt, va_list va) {
  char *buf = NULL;
  size_t rv = _vsnprintf(_term_writer, buf, (size_t)-1, fmt, va);
  console_flush();
  return rv;
}

int printf(const char *fmt, ...) {
  va_list va;
  va_start(va, fmt);
  size_t rv = vprintf(fmt, va);
  va_end(va);
  return rv;
}

////////////////////////////////////////////////////////////////////////////////
// assert.h
////////////////////////////////////////////////////////////////////////////////

extern "C" void __assert_fail(const char *assertion, const char *file,
                              unsigned int line, const char *function) {
  printf("%s:%u:%s(): assert(%s) failed\r\n", file, line, function, assertion);
  acpi::shutdown();
}

} // namespace nonstd
