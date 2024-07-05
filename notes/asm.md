# asm.md
Reminders for myself because I keep forgetting.

## cdecl x86 calling conventions
- Function arguments are passed on the stack in reverse order. This
  means the `(%esp)` is the return address, the first argument is
  `4(%esp)`, the second is `8(%esp)`, and so on (unless you push
  `%ebp`, then use these offsets from `%ebp` instead).
- Return values (if integer) are passed in `%eax`.
- Struct return values work by passing a hidden pointer argument. I
  try to avoid this if possible.
- `%eax`, `%ecx`, and `%edx` are caller-saved, the rest (`%ebx`,
  `%esp`, `%ebp`, `%edi`, `%esi`) are callee-saved.

## Logically equivalent operations
- `push N`: `sub $4, %esp; mov N, (%esp)`
- `pop N`: `mov (%esp), N; add $4, %esp`
- `call FN`: `push %eip; jmp FN`
- `ret`: `pop %eip`
- `enter N`: `push %ebp; mov %esp, %ebp; sub N, %esp`
- `leave`: `mov %ebp, %esp; pop %ebp`

## Addressing modes
Addressing modes in x86 can be summarized by [this Wikipedia
diagram](https://en.wikipedia.org/wiki/X86#Addressing_modes).

The syntax is fairly different between Intel/NASM and AT&T/GAS
syntax. E.g., `[%es:%ebp + 4]` in the former is `%es:4(%ebp)` in the
latter.
