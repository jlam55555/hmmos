#pragma once

/// \file
/// \brief Implementation of a small part of the C standard library, for
/// kernel use.
///
/// Not intended to be standards-compliant.

#include "libc_minimal.h"
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

extern "C" {
namespace nonstd {

////////////////////////////////////////////////////////////////////////////////
// ctype.h
////////////////////////////////////////////////////////////////////////////////
///
/// This declaration has to match that of ctype.h
int isprint(int c) __attribute__((nothrow));

////////////////////////////////////////////////////////////////////////////////
// string.h
////////////////////////////////////////////////////////////////////////////////

// Defined here so they can be constexpr (used in nonstd::string_view).
constexpr size_t strlen(const char *s) {
  const char *it = s;
  while (*it) {
    ++it;
  }
  return it - s;
}
constexpr int strcmp(const char *s1, const char *s2) {
  while (*s1 && *s2 && *s1 == *s2) {
    ++s1;
    ++s2;
  }
  return *s1 - *s2;
}
constexpr int strncmp(const char *s1, const char *s2, size_t n) {
  while (*s1 && *s2 && *s1 == *s2 && --n) {
    ++s1;
    ++s2;
  }
  return n ? *s1 - *s2 : 0;
}

////////////////////////////////////////////////////////////////////////////////
// stdio.h
////////////////////////////////////////////////////////////////////////////////
///
/// Note that these declarations have to match stdio.h. Even though we
/// don't link in the standard library, we _may_ use headers that pull
/// in stdio.h
int printf(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
int sprintf(char *s, const char *fmt, ...)
    __attribute((format(printf, 2, 3), nothrow));
int snprintf(char *s, size_t n, const char *fmt, ...)
    __attribute__((format(printf, 3, 4), nothrow));
int vprintf(const char *fmt, va_list);
int vsprintf(char *s, const char *fmt, va_list) __attribute__((nothrow));
int vsnprintf(char *s, size_t n, const char *fmt, va_list)
    __attribute__((nothrow));

////////////////////////////////////////////////////////////////////////////////
// assert.h
////////////////////////////////////////////////////////////////////////////////
//
// An implementation of `__assert_fail()`, which is required for the
// `assert()` macro, is provided.

} // namespace nonstd
}
