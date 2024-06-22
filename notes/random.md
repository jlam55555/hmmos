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

# binutils `-m` flag
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
