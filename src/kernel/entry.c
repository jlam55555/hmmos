static char *const _vid_buf = (char *)0xC00B8000;

static void _print(const char *str) {
  char *out = _vid_buf;
  while (*str) {
    *out = *(str++);
    out += 2;
  }
}

__attribute__((section(".text.entry"))) void _entry() {
  _print("We're in the kernel now!");

  for (;;) {
    __asm__("hlt");
  }
}
