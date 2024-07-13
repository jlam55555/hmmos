#pragma once

/// Implementation of a small part of the C standard library, for
/// kernel use. Not intended to be standards-compliant.

#include "libc_minimal.h"
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

////////////////////////////////////////////////////////////////////////////////
// ctype.h
////////////////////////////////////////////////////////////////////////////////
bool isprint(char c);

////////////////////////////////////////////////////////////////////////////////
// string.h
////////////////////////////////////////////////////////////////////////////////
size_t strlen(const char *s);
int strcmp(const char *s1, const char *s2);
int strncmp(const char *s1, const char *s2, size_t n);

////////////////////////////////////////////////////////////////////////////////
// stdio.h
////////////////////////////////////////////////////////////////////////////////
size_t printf(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
size_t sprintf(char *s, const char *fmt, ...)
    __attribute((format(printf, 2, 3)));
size_t snprintf(char *s, size_t n, const char *fmt, ...)
    __attribute__((format(printf, 3, 4)));
size_t vprintf(const char *fmt, va_list);
size_t vsprintf(char *s, const char *fmt, va_list);
size_t vsnprintf(char *s, size_t n, const char *fmt, va_list);
