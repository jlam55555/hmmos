#pragma once
/// These functions must be defined per the C specification.
/// The implementations are copied from
/// https://wiki.osdev.org/Limine_Bare_Bones.
#include <stddef.h>
#include <stdint.h>

void *memcpy(void *dest, const void *src, size_t n);
void *memset(void *s, int c, size_t n);
void *memmove(void *dest, const void *src, size_t n);
int memcmp(const void *s1, const void *s2, size_t n);
