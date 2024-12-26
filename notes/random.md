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

## .clangd configuration
Clangd configuration is ... not the best. First of all, it's
confusing. It can be configured via `compile_flags.txt`,
`compile_commands.json`, and/or `.clangd`.

The way I understand this currently is that clangd starts by looking
at the `.clangd` file (which can be in any parent directory of the
file you're compiling). This will then use the `compile_flags.txt`
file which contains default compilation options, or
`compile_commands.json` which contains concrete compilation options
for each target, in order to know which flags to compile with.

The problem with `compile_flags.txt` is that it's too general -- this
will apply to all files (you can't specify different compile flags for
C vs. C++ files for example). The problem with `compile_commands.json`
is that you have to generate it every time you want to use
it. (Presumably you can automate this, e.g., using
[`bear`](https://github.com/rizsotto/Bear)).

The [`.clangd` configuration file](https://clangd.llvm.org/config)
solves most of this problem. You can filter files using the
`PathMatch:` predicates to filter by C/C++ files. However adding an
`-Irelative/include/dir` is relative to the compiled file, not to the
project root, which is
[inconvenient](https://github.com/clangd/clangd/issues/1038).

## tmux vs. GNU screen
tl;dr: friendship ended with `screen`. now `tmux` is my best friend.

I had been using `screen` as my terminal multiplexor of choice since I
learned it from my mentor at Google (who had a similar emacs+screen
setup). It worked ... alright up until recently, when I discovered
various reasons why `tmux` seems to "just work" better OOTB. Some
specific anecdotal instances (and this only from a few days of using
`tmux` at work):

- I had a very bizarre bug at work where a program would only crash in
  `screen`. Turns out that it was messing with my maximum stack size
  -- the return value of `ulimit -s` was different inside and outside
  `screen`.
- `screen` is very slow pasting lines. Even a few (not very long)
  lines can take a few seconds. I thought this was an emacs thing or
  something to do with long lines until I tried it in `tmux` and it's
  just ... as fast as you would expect pasting text to be?
- `tmux` actually has amazing integration with iTerm with its
  so-called _control mode_. This means the mouse just "works" with
  click/drag/selections for managing windows and panes. However, this
  seems to have been [developed by the author of
  iTerm](https://unix.stackexchange.com/questions/453436/what-is-the-control-mode-in-tmux#comment1146454_525733)
  and Linux terminal emulator support is nonexistent.
- `tmux`'s scriptability seems more modern and intuitive. `screen` is
  also fairly scriptable but it seems to have a more limited set of
  commands.
- I've had fewer problems with using emacs in `tmux`. For example,
  colors were all screwed up in `screen` due to a
  `COLORTERM=truecolor` envvar, and comment background/foreground
  colors were swapped. Also, I had problems transmitting
  `M-<arrowkeys>` in `screen` but had no issue in `tmux`.
- `tmux` and `screen` sessions and windows are a little different. In
  `screen`, you can attach to the same session multiple times, and
  each instance of the session can be looking at different windows
  (tabs). In `tmux`, you can do this by creating new "grouped" session
  that shares the same windows. I.e., windows can be moved around or
  shared between sessions in `tmux`, which is pretty cool and seems
  more flexible.
- `tmux` and `screen` panes also are a bit different. In `screen` the
  panes are attached to a session; each pane displays one window of
  the session. (Multiple panes can display the same window). In `tmux`
  each window contains a set of one or more panes. It's also easy to
  navigate between panes using the `find-window` function.
- `tmux` also has the idea of servers and clients, although I haven't
  figured out how that works yet.

Luckily, the switch was pretty seamless. Both `tmux` and `screen` use
commands with an emacs-like prefix-key. I remap this to `C-o` since
that doesn't conflict with my usual emacs keybindings (as `screen`'s
`C-a` and `tmux`'s `C-b` both do).
