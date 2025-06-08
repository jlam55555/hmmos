#pragma once

/// \file
/// \brief A small subset of the C standard library that can also be
/// used in the bootloader.
///
/// This should be small (e.g., no printf() logic) and
/// contains definitions for some functions that may be implicitly
/// required by the C standard or standard library functions.

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
namespace nonstd {
extern "C" {
#define __inline inline
#else
#define __inline static inline
#endif

////////////////////////////////////////////////////////////////////////////////
// string.h
////////////////////////////////////////////////////////////////////////////////
// These are required by the C standard. Compilers may implicitly
// generate calls to these functions.
//
// TODO: These implementations are pretty unoptimized, doing
// comparisons byte-by-byte. This is pretty platform-agnostic, but we
// can speed this up significantly by going word-by-word since the
// endiannness is well-defined on x86 (or using SSE!).
//
// Hopefully the compiler is smart enough to inline or heavily
// optimize calls to these functions.
__inline void *memcpy(void *dest, const void *src, size_t n);
__inline void *memset(void *s, int c, size_t n);
__inline void *memmove(void *dest, const void *src, size_t n);
__inline int memcmp(const void *s1, const void *s2, size_t n);

// begin defs
__inline void *memcpy(void *dest, const void *src, size_t n) {
  uint8_t *pdest = (uint8_t *)dest;
  const uint8_t *psrc = (const uint8_t *)src;

  for (size_t i = 0; i < n; i++) {
    pdest[i] = psrc[i];
  }

  return dest;
}

__inline void *memset(void *s, int c, size_t n) {
  uint8_t *p = (uint8_t *)s;

  for (size_t i = 0; i < n; i++) {
    p[i] = (uint8_t)c;
  }

  return s;
}

__inline void *memmove(void *dest, const void *src, size_t n) {
  uint8_t *pdest = (uint8_t *)dest;
  const uint8_t *psrc = (const uint8_t *)src;

  if (src > dest) {
    for (size_t i = 0; i < n; i++) {
      pdest[i] = psrc[i];
    }
  } else if (src < dest) {
    for (size_t i = n; i > 0; i--) {
      pdest[i - 1] = psrc[i - 1];
    }
  }

  return dest;
}

__inline int memcmp(const void *s1, const void *s2, size_t n) {
  const uint8_t *p1 = (const uint8_t *)s1;
  const uint8_t *p2 = (const uint8_t *)s2;

  for (size_t i = 0; i < n; i++) {
    if (p1[i] != p2[i]) {
      return p1[i] < p2[i] ? -1 : 1;
    }
  }

  return 0;
}

#undef __inline
#ifdef __cplusplus
} // namespace nonstd
}
#endif
