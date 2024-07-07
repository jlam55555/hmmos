#pragma once

/// A small subset of the C standard library that can also be used in
/// the bootloader. This should be small (e.g., no printf() logic) and
/// contains definitions for some functions that may be implicitly
/// required by the C standard or standard library functions.

#include <stddef.h>

////////////////////////////////////////////////////////////////////////////////
// string.h
////////////////////////////////////////////////////////////////////////////////
/// These are required by the C standard. Compilers may implicitly
/// generate calls to these functions.
///
/// TODO: These implementations are pretty unoptimized, doing
/// comparisons byte-by-byte. This is pretty platform-agnostic, but we
/// can speed this up significantly by going word-by-word since the
/// endiannness is well-defined on x86 (or using SSE!).
void *memcpy(void *dest, const void *src, size_t n);
void *memset(void *s, int c, size_t n);
void *memmove(void *dest, const void *src, size_t n);
int memcmp(const void *s1, const void *s2, size_t n);

////////////////////////////////////////////////////////////////////////////////
// assert.h
////////////////////////////////////////////////////////////////////////////////
//
// An implementation of `__assert_fail()`, which is required for the
// `assert()` macro, is provided.
