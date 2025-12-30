/// \file init.cc
/// \brief Initial process.

extern "C" {

// DATA REGION
[[maybe_unused]] char data[4096 + 1024] = {1, 2, 3};

// BSS REGION
[[maybe_unused]] char bss[4096 * 2];

void _start() {
  int rval = 0;
  for (int i = 0; i < sizeof(bss); ++i) {
    bss[i] = i % 128;
    rval += bss[i];
  }

  // _exit(rval);
  __asm__("mov $0x01, %%eax;"
          "mov %0, %%ebx;"
          "int $0x80"
          :
          : "rm"(rval));
}

} // namespace "C"
