#pragma once

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <utility>

namespace nonstd {

// Implementation of Murmur hash for 32-bit size_t.
// Copied from libsupcxx
size_t hash_bytes(const void *ptr, size_t len, size_t seed = 0xc70f6907UL);

// Hash specializations
template <typename T> struct hash { size_t operator()(const T &key); };
template <std::convertible_to<size_t> T> struct hash<T> {
  size_t operator()(const T &key) { return key; }
};
template <typename T>
requires std::is_pointer_v<T>
struct hash<T> {
  size_t operator()(const T &key) {
    return reinterpret_cast<std::uintptr_t>(key);
  }
};
// TODO: add more hash specializations as necessary

// Hash algorithm copied from https://stackoverflow.com/a/2595226.
inline void _hash_combine(size_t &seed, size_t hash) {
  seed ^= hash + 0x9e3779b9 + (seed << 6) + (seed >> 2);
}

template <typename... Ts> size_t hash_combine(Ts &&...elems) {
  size_t acc = 0xDEADBEEFU;
  (_hash_combine(acc, hash<std::remove_cvref_t<Ts>>{}(std::forward<Ts>(elems))),
   ...);
  return acc;
}

} // namespace nonstd
