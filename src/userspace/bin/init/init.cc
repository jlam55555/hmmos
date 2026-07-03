/// \file init.cc
/// \brief Initial process.

#include "jlibc/unistd.h"

extern "C" {

// DATA REGION
[[maybe_unused]] char data[4096 + 1024] = {1, 2, 3};

// BSS REGION
[[maybe_unused]] char bss[4096 * 2];

void _start() {
  int rval = 0;
  for (int i = 0; i < sizeof data; ++i) {
    rval += data[i];
  }
  for (int i = 0; i < sizeof bss; ++i) {
    rval += bss[i];
  }

  exit(rval);
}

} // namespace "C"
