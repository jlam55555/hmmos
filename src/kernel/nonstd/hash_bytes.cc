#include "nonstd/hash_bytes.h"
#include "libc_minimal.h"

namespace {

// https://github.com/martinus/robin-hood-hashing/issues/36
template <typename T> T unaligned_load(void const *ptr) noexcept {
  // using memcpy so we don't get into unaligned load problems.
  // compiler should optimize this very well anyways.
  T t;
  nonstd::memcpy(&t, ptr, sizeof(T));
  return t;
}

} // namespace

namespace nonstd {

// Implementation of Murmur hash for 32-bit size_t.
// Copied from libsupcxx
size_t hash_bytes(const void *ptr, size_t len, size_t seed) {
  const size_t m = 0x5bd1e995;
  size_t hash = seed ^ len;
  const char *buf = static_cast<const char *>(ptr);

  // Mix 4 bytes at a time into the hash.
  while (len >= 4) {
    size_t k = unaligned_load<size_t>(buf);
    k *= m;
    k ^= k >> 24;
    k *= m;
    hash *= m;
    hash ^= k;
    buf += 4;
    len -= 4;
  }

  size_t k;
  // Handle the last few bytes of the input array.
  switch (len) {
  case 3:
    k = static_cast<unsigned char>(buf[2]);
    hash ^= k << 16;
    [[fallthrough]];
  case 2:
    k = static_cast<unsigned char>(buf[1]);
    hash ^= k << 8;
    [[fallthrough]];
  case 1:
    k = static_cast<unsigned char>(buf[0]);
    hash ^= k;
    hash *= m;
  };

  // Do a few final mixes of the hash.
  hash ^= hash >> 13;
  hash *= m;
  hash ^= hash >> 15;
  return hash;
}

} // namespace nonstd
