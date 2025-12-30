#include "unistd.h"

void exit(int status) {
  __asm__("mov $0x01, %%eax;"
          "mov %0, %%ebx;"
          "int $0x80"
          :
          : "rm"(status));
  __builtin_unreachable();
}
