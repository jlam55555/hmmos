#pragma once
/// These functions must be defined per the C specification.
/// These implementations are copied from
/// https://wiki.osdev.org/Limine_Bare_Bones.
///
/// TODO: These implementations are pretty unoptimized, doing
/// comparisons byte-by-byte. This is pretty platform-agnostic, but we
/// can speed this up significantly by going word-by-word since the
/// endiannness is well-defined on x86 (or using SSE!).
#include <stddef.h>
#include <stdint.h>

void *memcpy(void *dest, const void *src, size_t n);
void *memset(void *s, int c, size_t n);
void *memmove(void *dest, const void *src, size_t n);
int memcmp(const void *s1, const void *s2, size_t n);
