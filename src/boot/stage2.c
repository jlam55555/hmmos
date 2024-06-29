const char *text = "text in C code\r\n";

[[noreturn, gnu::naked]] void c_entry(void) {
  // This is no better than an asm function, just testing
  // bootstrapping to C.
  __asm__ volatile("mov %0,%%di\n\tcall print" : : "m"(text));
  __asm__ volatile("mov $msg,%di\n\tcall print\n\tjmp done");
}
