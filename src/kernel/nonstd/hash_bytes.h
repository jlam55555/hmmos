#pragma once

#include <concepts>
#include <cstddef>

namespace nonstd {

// Implementation of Murmur hash for 32-bit size_t.
// Copied from libsupcxx
size_t hash_bytes(const void *ptr, size_t len, size_t seed = 0xc70f6907UL);

// Hash specializations
template <typename T> struct hash { size_t operator()(const T &key); };
template <std::convertible_to<size_t> T> struct hash<T> {
  size_t operator()(const T &key) { return key; }
};
// TODO: add more hash specializations as necessary

} // namespace nonstd
