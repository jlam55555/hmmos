# random notes
Or more like a set of rants

## asmfmt
Every time I save it eats away one layer of `;` on an otherwise empty
line. So if I have a line with just `;;;`, after calling
`format-all` it'll become `;;`. Not that AT&T syntax uses `;`
anyways, but still.

Also, it has trouble parsing the line `jmp .` -- the indentation is
all messed up after that. Maybe another issue with AT&T syntax.

For now I'll manually format my asm.

## Cool emacs things
I just learned about `fill-paragraph` and `auto-fill-mode`. Useful for
textual files (`*.md`) and prog-mode files that don't have a different
formatter (or whose formatters don't care about long lines).

## binutils `-m` flag
binutils is the package that supplies common build tools like `ld` and
`objdump`. They have a funky way of specifying architectures known as
[BFD](https://en.wikipedia.org/wiki/Binary_File_Descriptor_library). You
can mess around with this by cloning the binutils repo, running
`./configure`, and then playing around with the `config.sub` script
which produces a canonical BFD.

E.g., you can pass `x64`, `i386:x86`, `i386:i386`, and `i386:i386` to
it. IIRC `x64` is a.k.a. x86_64 or Intel 64 or amd64 (but not IA64,
that's Itanium), while `i386` and `x86` refer to the same thing. (To
confuse things further, there's a thing referred to as IA-32e in the
Intel SDM, but this is a 32-bit compatibility mode in x64_64 rather
than its own architecture.)

What's utterly confusing is that the architectures supported by
objdump's `-m` flag are different. E.g., you can specify `i386`,
`i386`, but x64 is `i386:x86_64`, which doesn't make much sense to me.

Well, not that I'm building it in 64-bit mode now.

## gdb and lldb notes
I'm able to do most of what I want to do (poking at registers and
disassembly). There's also a `gui` command which gives you something
like gdb's `layout` commands, and a `gdb-remote` command similar to
gdb's remote targets. The commands feel less archaic than GDB's.

I find that lldb chokes when I try to step one instruction for a `jmp
.` instruction, while gdb works.

## i8086 (real mode) support
Currently GDB's `set architecture i8086` command to disassemble
real-mode instructions [seems
broken](https://sourceware.org/bugzilla/show_bug.cgi?id=22869). (I'm
using GDB 14, seems broken at least as of GDB 8.) [This
workaround](https://gitlab.com/qemu-project/qemu/-/issues/141#note_567553482)
works though.

This is better than lldb though, where I don't see i8086 support at
all. In this case you'll need to `objdump` (if you just want to look
at the disassembly) or just switch to gdb (if you need the disassembly
to be correct for stepping). It doesn't seem that LLVM is popular for
OSDev so I'll just accept this.

## clang version support
I originally started out this project on a laptop running arch linux,
which currently has support for LLVM 17. I tried running this on a
debian bullseye (11) laptop, which only supports LLVM 11. There were a
lot of issues on the older version -- some of the cmdline arguments
were different (for example, `ld.lld --oformat=binary --build_id=none`
and `ld.lld --oformat=binary --build_id none` didn't work, but `ld.lld
--oformat binary --build_id=none` did) and the linker simply did not
put output sections in the right location.

I thought about building LLVM 17 on my own but decided to try
upgrading debian first. Upgrading to bullseye (12) gives me LLVM 14,
which did pretty much work OOTB (save for the fact that we lose _some_
features such as new-style `[[ attribute ]]`s).

In terms of C++ support (which I care about, since I'm planning for
the majority of the kernel to be written in C++), clang 14
[supports](https://clang.llvm.org/cxx_status.html) the main features
of C++17 and C++20 that I care about (C++17 structured bindings, C++17
CTAD, and (most of) C++20 concepts).

I don't currently plan to build my own cross-compiler for now since
clang [inherently supports
cross-compiling](https://stackoverflow.com/a/72254698), but maybe a
need will arise in the future. For now enforcing clang >= 14 is good
enough for this project.

## QEMU x86 quirks
Both of the following make my life easier when running in QEMU, but
make it harder for me to handle the expected case for real hardware.

- *A20 is set on boot*: To be fair, the OSDev wiki states that this is
  very non-standardized and there are a myriad of ways to
  enable/disable the A20 line depending on the hardware. In QEMU's
  case, [it was already set on
  boot](https://forum.osdev.org/viewtopic.php?f=1&t=26256).
- *No data segment offset limit checks*: In real mode, I noticed I can
  write to 0x0000:0xB8000 without having to use unreal
  mode. Similarly, when in protected mode/unreal mode, I was unable to
  trigger the segment check (which should cause a
  [GPF](https://wiki.osdev.org/Exceptions#General_Protection_Fault))
  even if I manually set the limits low. It seems that QEMU doesn't
  perform the limit checks ([at least as of
  2019](https://lists.gnu.org/archive/html/qemu-devel/2019-02/msg06518.html)).
  I do get GPFs for the code segment though.
