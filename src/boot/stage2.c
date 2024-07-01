/// This file is currently still run in real mode. Later on let's have
/// the real mode work all done in asm and only transition to C in
/// protected mode since (afaik) there isn't a well-defined 16-bit
/// calling convention.

/// This file is currently unused.
const char *text = "Welcome from C code.\r\n";

__attribute__((noreturn, naked)) void c_entry(void) {
  // This is no better than an asm function, just testing
  // bootstrapping to C.
  __asm__ volatile("mov %0,%%di\n\tcall print\n\tjmp done" : : "m"(text));
}
