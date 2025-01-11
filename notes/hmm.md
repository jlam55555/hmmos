# hmm.md
Interesting observations and debugging sessions

## slow memory speed
When I was first experimenting with my kernel thread implementation, I
came across an <strike>frustrating</strike> interesting result: some
threads were absurdly faster than others at doing the same
deterministic tasks.

It turns out only the initial kernel thread was slow. It was slow due
to operations on its stack. The stack was allocated in the bootloader,
unlike the stack for other kernel threads which is allocated via a
simple sequential arena. The kernel stack is allocated just under the
MBR bootloader load address (physical address 0x7C00, HHDM virt
address 0xC0007C00), while other threads' stacks are allocated on some
higher page-aligned memory address.

Here's some test code to illustrate this:

```
#define change_stk(stk) __asm__ volatile("mov %0, %%esp" : : "rm"(stk))

void print_stk() {
  void *stk;
  __asm__ volatile("mov %%esp, %0" : "=m"(stk));
  nonstd::printf("stk=%x\r\n", (unsigned)stk);
}

void do_something() {
  constexpr unsigned n = 1'000'000;
  int j = 1;
  uint64_t start = arch::time::rdtsc();
  for (int i = 0; i < n; ++i) {
    j *= 2;
  }
  uint64_t end = arch::time::rdtsc();
  nonstd::printf("cycles/op=%llu\r\n", (end - start) / n);
}

__attribute__((section(".text.entry"))) void _entry() {
  // Stack is originally just under the bootloader.
  print_stk();
  do_something();

  // Change stack to a random address on a different page.
  change_stk((char *)0xC000B000);
  print_stk();
  do_something();

  // Change it back just to make sure it wasn't really slow the first
  // time.
  change_stk((char *)0xC0007bd0);
  print_stk();
  do_something();

  acpi::shutdown();
}
```

Output (compiled with clang with -O1):

```
stk=c0007bd0
cycles/op=3060
stk=c000afe0
cycles/op=8
stk=c0007bb0
cycles/op=3068
```

Just to make sure this wasn't something wrong with my timing function
(rdtsc), I tried the following example, which shouldn't involve any
memory operations:

```
void do_something() {
  nonstd::printf("time between rdtsc=%llu\r\n",
                 -(arch::time::rdtsc() - arch::time::rdtsc()));
}
```

Output:

```
stk=c0007bd0
time between rdtsc=10963
stk=c000afe0
time between rdtsc=190
stk=c0007bb0
time between rdtsc=3306
```

Huh? Is my rdtsc screwed? Let's look at the disassembly:

```
$ objdump -CDr out.O1/kernel/entry.o
...
00000030 <do_something()>:
  30:   55                      push   %ebp
  31:   89 e5                   mov    %esp,%ebp
  33:   56                      push   %esi
  34:   83 ec 14                sub    $0x14,%esp
  37:   e8 fc ff ff ff          call   38 <do_something()+0x8>
                        38: R_386_PC32  arch::time::rdtsc()
  3c:   89 c6                   mov    %eax,%esi
  3e:   89 55 f8                mov    %edx,-0x8(%ebp)
  41:   e8 fc ff ff ff          call   42 <do_something()+0x12>
                        42: R_386_PC32  arch::time::rdtsc()
  46:   89 c1                   mov    %eax,%ecx
  48:   8b 45 f8                mov    -0x8(%ebp),%eax
  4b:   29 f1                   sub    %esi,%ecx
  4d:   19 c2                   sbb    %eax,%edx
  4f:   89 e0                   mov    %esp,%eax
  51:   89 50 08                mov    %edx,0x8(%eax)
  54:   89 48 04                mov    %ecx,0x4(%eax)
  57:   c7 00 09 00 00 00       movl   $0x9,(%eax)
                        59: R_386_32    .rodata.str1.1
  5d:   e8 fc ff ff ff          call   5e <do_something()+0x2e>
                        5e: R_386_PC32  printf
  62:   83 c4 14                add    $0x14,%esp
  65:   5e                      pop    %esi
  66:   5d                      pop    %ebp
  67:   c3                      ret
```

So it turns out we store a temporary between the two calls at
instruction 3e (writing to `-0x8(%ebp)`. Let's optimize this away by
hand.

```
void do_something() {
  uint64_t res;
  __asm__ volatile("rdtsc\n\t"
                   "mov %%eax, %%esi\n\t"
                   "mov %%edx, %%edi\n\t"
                   "rdtsc\n\t"
                   "sub %%esi, %%eax\n\t"
                   "sbb %%edi, %%edx\n\t"
                   "mov %%eax, %0\n\t"
                   "mov %%edx, %1\n\t"
                   : "=m"(*(uint32_t *)&res), "=m"(*(((uint32_t *)&res) + 1))
                   :
                   : "%eax", "%edx", "%esi", "%edi");
  nonstd::printf("time between rdtsc=%llu\r\n", res);
}
```

Output:

```
stk=c0007bd0
time between rdtsc=76
stk=c000afe0
time between rdtsc=19
stk=c0007bb0
time between rdtsc=38
```

Ok, much better. So the memory (stack) operation is indeed the
culprit. Let's continue to narrow down the problem. Which memory
operations are slow? Let's iterate page-by-page over the first 1GB of
memory (since this is mapped by the bootloader in the HHDM).

```
void do_write(size_t addr) {
  constexpr int n = 10'000;
  uint64_t start = arch::time::rdtsc();
  for (int i = 0; i < n; ++i) {
    // Write to the memory address and then restore the value so we
    // don't blow up the system.
    //
    // Note that we can't do `mov (%0), (%0)` since memory- memory
    // operations are disallowed in x86.
    //
    // Note also that just reading the value doesn't seem to
    // experience a slowdown, only writing the value. So `mov (%0),
    // %eax` does not experience the slowdown.
    __asm__ volatile("mov %0, %%eax\n\t"
                     "mov %%eax, %0"
                     : "=m"(*(int *)addr)
                     :
                     : "eax");
  }
  uint64_t end = arch::time::rdtsc();
  if (auto cycles_per_op = (end - start) / n; cycles_per_op >= 500) {
    nonstd::printf("addr=%x cycles/op=%llu\r\n", addr, cycles_per_op);
  }
}

__attribute__((section(".text.entry"))) void _entry() {
  // We've established stack is slow, so let's go somewhere else just
  // to speed things up a bit.
  change_stk(0xDEADBEEF);

  constexpr size_t hhdm_start = 0xC0000000;
  constexpr size_t hhdm_end = 0x00000000; // overflow
  constexpr size_t pg_sz = 4096;

  for (size_t pg = hhdm_start; pg != hhdm_end; pg += pg_sz) {
    do_write(pg);
  }

  acpi::shutdown();
}
```

Output:

```
$ make run OPT=1
addr=c0007000 cycles/op=1512
addr=c0008000 cycles/op=4103
addr=c0009000 cycles/op=633
addr=c00bf000 cycles/op=2649
addr=c00e9000 cycles/op=2005
addr=c00ea000 cycles/op=936
addr=c00eb000 cycles/op=2112
addr=c00ec000 cycles/op=1276
addr=c00ed000 cycles/op=731
addr=c00ee000 cycles/op=1207
addr=c00ef000 cycles/op=889
addr=c0405000 cycles/op=989
addr=c0407000 cycles/op=1180
addr=c0408000 cycles/op=914
addr=c0409000 cycles/op=511
addr=fe005000 cycles/op=1003
addr=fe007000 cycles/op=1188
addr=fe008000 cycles/op=942
addr=fe009000 cycles/op=511
```

I'm able to get this result pretty consistently when running this
multiple times. Now here's where things get really interesting: the
results change when compiling with different options (using gcc, or
switching optimization levels). It's a little nuanced but the results
are consistent.

```
$ make run OPT=1 GNU=1
addr=c0007000 cycles/op=1550
addr=c0008000 cycles/op=4097
addr=c00e9000 cycles/op=1899
addr=c00ea000 cycles/op=913
addr=c00eb000 cycles/op=2135
addr=c00ec000 cycles/op=1295
addr=c00ed000 cycles/op=730
addr=c00ee000 cycles/op=1227
addr=c00ef000 cycles/op=904
addr=c0404000 cycles/op=1006
addr=c0405000 cycles/op=1377
addr=c0407000 cycles/op=566
addr=fe004000 cycles/op=1011
addr=fe005000 cycles/op=1383
addr=fe007000 cycles/op=575
```

The 500 threshold is chosen to illustrate the following point. Most of
the reads/writes are much quicker. Here are the cycles/loop for more
typical memory pages.

```
addr=c0d35000 cycles/op=12
addr=c0d36000 cycles/op=12
addr=c0d37000 cycles/op=12
addr=c0d38000 cycles/op=12
addr=c0d39000 cycles/op=12
addr=c0d3a000 cycles/op=12
addr=c0d3b000 cycles/op=12
addr=c0d3c000 cycles/op=12
addr=c0d3d000 cycles/op=12
```

There are some pretty telling details here! First of all, you can see
that the memory page containing the original slow stack (0xC0007000)
is on this list.

But before I talk about the interesting relevations, I first want to
check two things.

Firstly, does this slowness affect reads and/or
writes? We can modify the above snippet to only do the read (safe) or
only do the write (unsafe since we're overwriting arbitrary memory,
which could include the stack or text regions). It turns out that only
the write is affected. Reads consistently take 5 cycles/op and never
cause these outliers, whereas writes usually take 7 cycles/op but form
the peaks shown above.

Secondly, does this slowness happen at the page level or something
more granular? Let's look at pages 0xC0006000 (fast), 0xC0007000
(slow), 0xC0008000 (even slower). Let's update the main loop:

```
for (size_t addr = 0xC0006000; addr < 0xC0009000; addr += 512) {
  do_write(addr);
}
```

Output:

```
addr=c0006000 cycles/op=16
addr=c0006200 cycles/op=12
addr=c0006400 cycles/op=12
addr=c0006600 cycles/op=11
addr=c0006800 cycles/op=12
addr=c0006a00 cycles/op=12
addr=c0006c00 cycles/op=12
addr=c0006e00 cycles/op=12
addr=c0007000 cycles/op=1553
addr=c0007200 cycles/op=1541
addr=c0007400 cycles/op=1565
addr=c0007600 cycles/op=1543
addr=c0007800 cycles/op=1554
addr=c0007a00 cycles/op=1550
addr=c0007c00 cycles/op=1533
addr=c0007e00 cycles/op=1505
addr=c0008000 cycles/op=4145
addr=c0008200 cycles/op=4112
addr=c0008400 cycles/op=4114
addr=c0008600 cycles/op=4063
addr=c0008800 cycles/op=4021
addr=c0008a00 cycles/op=3971
addr=c0008c00 cycles/op=3957
addr=c0008e00 cycles/op=3937
```

It turns out this is very page-oriented behavior. This is enough to
form a hypothesis. Here are some facts.

1. The slowness occurs only on writes and happens on a per-page
   basis. The only hardware-related page slowness is page faults, but
   this should only happen on the first page access and shouldn't be
   noticeable when averaged over many loop cycles.
2. The general behavior is consistent across compilers and
   optimization levels, which indicates that this is not a software
   inefficiency or bug.
3. All of the slow pages are special:
   - 0xC0007000 and following pages: These are bootloader pages. The
     initial 512-byte MBR sector is loaded at 0x7C00 (0xC0007C00 in
     the HHDM), and the following 63 sectors are copied in by the
     bootloader immediately afterwards in low memory. Incidentally,
     the kernel stack coinhabits the page 0xC0007000 -- it starts at
     0xC0006FFF and grows downwards.
   - 0xC00B8000-0xC00EF000: These are standard hardware-mapped (I/O)
     memory. The OSDev wiki has a [good
     summary](https://wiki.osdev.org/Memory_Map_(x86)#Overview). It
     makes sense that these are slow, so we can ignore them for the
     sake of this discussion.
   - 0xC0400000 and following pages: The bootloader copies the kernel
     to the first unused hugepage in low memory (0x400000 physical
     address, 0xC0400000 in the HHDM).
   - 0xFE000000 and following pages: These are the kernel virtual
     mapping and also map to the physical address 0x400000.
4. Which exact pages are slow in the kernel mapping region depends on
   the compiler options and choice of compiler.
5. The slowness only happens for writes, not reads.

(1) and (2) point to this being a feature of the QEMU emulator. (3)
and (4) indicate that the behavior relates to the BIOS and kernel text
regions. So the hypothesis is that writing anywhere on a page
containing executable instructions in QEMU is slow.

Well, how does QEMU know that code is executable? Well, I guess that
the code is only slow if we've actually executed instructions on that
memory page. This of course depends on the compiler and compiler
options, which explains (4).  We can check this by stepping through
the debugger and seeing which instructions we run. Since we have such
a simple example, it should be easy to step through the program. In
the case of clang with -O1, we see that we indeed execute kernel code
on the slow kernel pages:

```
fe000000 _entry
fe005000 do_write
fe007000 printf code
fe008000 more printf code
fe009000 more printf code
```

(Yes, there's a lot of printf code there.) To test this hypothesis
further, let's try to avoid calling the printf code until after we've
done all the benchmarking. Then it shouldn't show up as a slow page,
right? Seems that's the case:

```
addr=c0007000 cycles/op=1547
addr=c0008000 cycles/op=4112
addr=c0009000 cycles/op=646
addr=c00b8000 cycles/op=407
addr=c00b9000 cycles/op=317
addr=c00ba000 cycles/op=320
addr=c00bb000 cycles/op=317
addr=c00bc000 cycles/op=307
addr=c00bd000 cycles/op=304
addr=c00be000 cycles/op=306
addr=c00bf000 cycles/op=307
addr=c00e9000 cycles/op=1858
addr=c00ea000 cycles/op=896
addr=c00eb000 cycles/op=2104
addr=c00ec000 cycles/op=1268
addr=c00ed000 cycles/op=733
addr=c00ee000 cycles/op=1205
addr=c00ef000 cycles/op=882
addr=c0400000 cycles/op=302
addr=c0405000 cycles/op=398
addr=fe000000 cycles/op=305
addr=fe005000 cycles/op=401
```

Importantly, the pages with the `printf()` implementation aren't slow
anymore! Interestingly, I had to turn the threshold down to 300 to see
the effect, which was very reproducible. Which brings me to the next
question: is the slowness a function of the number of executable lines
executed? That might make sense given that some pages are hugely
slower than others. And any minor code changes may cause the compiled
instructions to be shuffled around, causing page writes to be
relatively slower or faster.

For the final experiment, let's control the amount of executed
instructions that lie on a page, and see how the write speed is
affected. Here's the whole test program:

```
#define change_stk(stk) __asm__ volatile("mov %0, %%esp" : : "rm"(stk))

void do_write(char *addr) {
  constexpr int n = 10000;
  uint64_t start = arch::time::rdtsc();
  for (int i = 0; i < n; ++i) {
    __asm__ volatile("movb $0, %0" : : "m"(*addr));
  }
  uint64_t end = arch::time::rdtsc();
  nonstd::printf("cycles/op=%llu\r\n", (end - start) / n);
}

__attribute__((section(".text.entry"))) void _entry() {
  // We've established stack is slow, so let's go somewhere else just
  // to speed things up a bit.
  change_stk(0xDEADBEEF);

  constexpr size_t page_sz = 4096;

  // Choose a random page that should be empty on startup.
  char *const pg = (char *)0xD0000000;
  nonstd::printf("Writing to fresh, crunchy ram...\t");
  do_write(pg);

  // Now let's fill up this page with `ret` (0xC3) instructions.
  // Except for the first byte, which we'll leave alone since
  // it'll keep getting overwritten.
  nonstd::printf("Filling RAM with executable instructions...\t");
  nonstd::memset(pg + 1, 0xC3, page_sz - 1);
  do_write(pg);

  // Now let's execute some code on this page and see if writing to
  // the page becomes slower.
  for (unsigned i = 1; i < page_sz; ++i) {
    __asm__ volatile("call *%0" : : "r"(pg + i));
    if ((i + 1) % 128 == 0) {
      nonstd::printf("Executed %u/%u bytes...\t", i, page_sz - 1);
      do_write(pg);
    }
  }

  // Let's try slowly overwriting the instructions and see if things
  // are faster again.
  for (unsigned i = 1; i < page_sz; ++i) {
    // Overwrite it with the same bytes as before.
    // It doesn't really matter what we overwrite it with though.
    *(char *)(pg + i) = 0xC3;
    if ((i + 1) % 128 == 0) {
      nonstd::printf("Cleared %u/%u bytes...\t", i, page_sz - 1);
      do_write(pg);
    }
  }

  acpi::shutdown();
}
```

The results are exactly as hypothesized:

```
Writing to fresh, crunchy ram...        cycles/op=91
Filling RAM with executable instructions...     cycles/op=12
Executed 127/4095 bytes...      cycles/op=2395
Executed 255/4095 bytes...      cycles/op=4583
Executed 383/4095 bytes...      cycles/op=6735
Executed 511/4095 bytes...      cycles/op=8949
Executed 639/4095 bytes...      cycles/op=11053
...
Executed 3839/4095 bytes...     cycles/op=66152
Executed 3967/4095 bytes...     cycles/op=68458
Executed 4095/4095 bytes...     cycles/op=70473
Cleared 127/4095 bytes...       cycles/op=68180
Cleared 255/4095 bytes...       cycles/op=65913
Cleared 383/4095 bytes...       cycles/op=63619
...
Cleared 3583/4095 bytes...      cycles/op=8908
Cleared 3711/4095 bytes...      cycles/op=6766
Cleared 3839/4095 bytes...      cycles/op=4636
Cleared 3967/4095 bytes...      cycles/op=2427
Cleared 4095/4095 bytes...      cycles/op=12
```

So now we've gandered a guess for this slow behavior and it turned out
to be right, but ... why? My best guess is that QEMU checks if you've
overwritten any instructions so it can clear the icache, allowing for
the expected behavior when doing more unconventional things like
[runtime code
modification](https://jpassing.com/2015/01/19/runtime-code-modification-explained-part-2-cache-coherency-issues/). There's
only a cost if we try to write to a page with executable instructions,
which shouldn't happen much of the time (text and data/stack/heap
regions are typically on separate pages with different memory
protections).

After the initial investigation I found that QEMU does not use KVM by
default. Turning KVM on (`-accel kvm`) shows that this behavior is
indeed due to QEMU emulation and not the Linux/hardware
virtualization. Which makes sense given there's no way the hardware is
complicated enough to store which bytes in a page are executable
bytes.

```
Writing to fresh, crunchy ram...        cycles/op=63
Filling RAM with executable instructions...     cycles/op=2
Executed 127/4095 bytes...      cycles/op=2
Executed 255/4095 bytes...      cycles/op=2
...
Executed 3967/4095 bytes...     cycles/op=2
Executed 4095/4095 bytes...     cycles/op=2
Cleared 383/4095 bytes...       cycles/op=2
Cleared 511/4095 bytes...       cycles/op=2
...
Cleared 3967/4095 bytes...      cycles/op=2
Cleared 4095/4095 bytes...      cycles/op=2
```

I didn't find the exact QEMU code but [this blog
post](https://airbus-seclab.github.io/qemu_blog/tcg_p3.html) hints at
how QEMU converts its load/store IR opcodes into the native x86
opcodes.

The solution to my original problem is to move the stack and
(writable) data regions of the kernel onto different pages.

This unexpectedly ended up solving a different but related performance
problem. Previously, setting up the page frame array was very
slow. Compiling with -O1 helped tremendously (runtime went from ~1min
to <1s) and I figured this was due to optimizing the memcpy/memset
functions. However, it turns out this operation was simply slow due to
the abundance of stack operations -- compiling with -O1 probably
avoided writing an intermediate value to the stack in one of these
functions. By moving the bootloader stack to a different page, I am
able to get similar performance without -O1.

Nice!

### slowness after hlt

Another issue I noticed when turning on pre-emptive scheduling is that
the `schedule()` call was much slower when pre-emptive scheduling
(from the timer interrupt) as opposed to co-operative scheduling
(which was essentially being done in a tight loop from the code).

My guess is that the icache gets cold after not accessing it for some
time. This is confirmed via a simple experiment:

```
// stolen from wikipedia
unsigned int isqrt(unsigned int y) {
  unsigned int L = 0;
  while ((L + 1) * (L + 1) <= y) {
    L = L + 1;
  }
  return L;
}

void do_benchmark() {
  uint64_t start = arch::time::rdtsc();
  // Random computation to benchmark.
  isqrt(314159265);
  uint64_t end = arch::time::rdtsc();
  nonstd::printf("cycles=%llu\r\n", end - start);
}

__attribute__((section(".text.entry"))) void _entry() {
  crt::run_global_ctors();

  // Enable interrupts; we need the PIT timer to wake us from hlt.
  arch::idt::init();

  for (int i = 0; i < 3; ++i) {
    for (int j = 0; j < 3; ++j) {
      do_benchmark();
    }
    nonstd::printf("hlt\r\n");
    __asm__ volatile("hlt");
  }

  acpi::shutdown();
}
```

Output:

```
cycles=258989
cycles=189297
cycles=189221
hlt
cycles=206150
cycles=220419
cycles=190475
hlt
cycles=254999
cycles=230698
cycles=161823
hlt
```

The same results with KVM enabled:

```
Enabling interrupts...
cycles=54872
cycles=30286
cycles=42161
hlt
cycles=70262
cycles=30286
cycles=30495
hlt
cycles=70281
cycles=30267
cycles=30476
hlt
```

I'm not sure exactly what's going on that causes the icache to get
cold -- no other code is running on the OS in the meantime. I imagine
QEMU shares the icache with the guest system and it gets cold when the
guest system `hlt`s. With the QEMU (non-KVM) runtime, I imagine
there's enough other code running to cause the icache to become even
more stale.

## KVM memory invalidation heisenbug
I thought QEMU used KVM virtualization by default until recently when
I learned it has its [own translation
layers](https://airbus-seclab.github.io/qemu_blog/tcg_p1.html). When I
tried to turn on KVM (`-accel kvm`), I got some weird issues with the
page mapping.

There's a check in the bootloader that the HHDM is set up
properly. This checks that the 1MB of memory starting from 0xC0000000
(high-half direct mapping) matches the 1MB starting from 0x0 (direct
mapping). We can only check up to 1MB since the lower-half direct
mapping is only 1MB for bootstrapping from real mode. We actually do
this check twice: once before the page table is set up (check should
fail) and once afterwards (check should succeed).

With KVM acceleration turned on, the second check started failing.

I had just set up debugging with symbols at this point, so I tried
turning that on. Turns out that breakpoints don't quite work OOTB with
KVM so you need [a little
hack](https://forum.osdev.org/viewtopic.php?p=315671&sid=ab78e42403df71a73a191a03d238209a#p315671). This hack worked really well.

Now the problem was that the error stopped showing up when running in
the debugger, even with no breakpoints installed! Huh. It also only
shows up in some build variants when not running in the debugger:
clang with default optimization level doesn't cause a failure. Every
other build variant causes the failure.

Even weirder, when I put a breakpoint in the comparison failure in the
`memcmp` call, the compared values show as equal. Here's the
unadultered `memcmp` implementation:

```
int memcmp(const void *s1, const void *s2, size_t n) {
  const uint8_t *p1 = (const uint8_t *)s1;
  const uint8_t *p2 = (const uint8_t *)s2;

  for (size_t i = 0; i < n; i++) {
    if (p1[i] != p2[i]) {
      return p1[i] < p2[i] ? -1 : 1;
    }
  }

  return 0;
}
```

If I put a print statement, `p1[i]` and `p2[i]` show as equal:

```
    if (p1[i] != p2[i]) {
      console_printb(p1[i]);
      console_printb(p2[i]);
      console_printl((unsigned)&p1[i]);
      console_printl((unsigned)&p2[i]);
      return p1[i] < p2[i] ? -1 : 1;
    }

// outputs: 0x23 0x23 0x00002008 0xC0002008
```

This happens at the same addresses (2008h) for all build variants with
failures.

Let's continue this weirdness. If I check for equality again after the
print statements, they now compare as equal, causing the check to
pass.

```
    if (p1[i] != p2[i]) {
      console_printb(p1[i]);
      console_printb(p2[i]);
      console_printl((unsigned)&p1[i]);
      console_printl((unsigned)&p2[i]);
      if (p1[i] == p2[i]) {
        continue; // we hit this
      }
      return p1[i] < p2[i] ? -1 : 1;
    }

// outputs same as before but check now passes
```

Which means that p1[i] and/or p2[i] changed between the first and
second comparison. Now if I take out the print statements, the second
comparison still shows these as unequal.

```
    if (p1[i] != p2[i]) {
      if (p1[i] == p2[i]) {
        continue; // we don't hit this ... ??? !!!
      }
      return p1[i] < p2[i] ? -1 : 1;
    }

// check fails!
```

This feels like a true heisenbug scenario. Oberving the memory (via a
print statement, or by running in a debugger) changes the
behavior. The only way I was able to show the difference in values was
by storing the value to a temporary.

```
    uint8_t c1 = p1[i];
    uint8_t c2 = p2[i];
    if (c1 != c2) {
      console_printb(c1);
      console_printb(c2);
      console_printl((unsigned)&p1[i]);
      console_printl((unsigned)&p2[i]);
      return c1 < c2 ? -1 : 1;
    }

/// output: 0x23 0x03 0x00002008 0xC0002008
```

So, at the time of the comparison, these values are indeed
different. But something changes it soon afterwards. This seems like
an issue with caching -- in particular the page mapping cache (the
TLB). The TLB cache _should_ be completely flushed when we install the
page table by [setting the `%cr3`
register](https://wiki.osdev.org/TLB#Modification_of_paging_structures),
but let's try flushing it manually.

```
  for (size_t i = 0; i < n; i++) {
    __asm__ volatile("invlpg %0\n\t"
                     "invlpg %1\n\t"
                     :
                     : "m"(p1[i]), "m"(p2[i]));
    if (p1[i] != p2[i]) {
      return p1[i] < p2[i] ? -1 : 1;
    }
  }

// this works! but...
```

This _seems_ to work, but it seems more like a false positive than
anything. Really, only flushing the page for `p2[i]` should make a
difference since its value is stale (0x03 should be 0x23), but this
_also_ works if we only flush the TLB entry for `p1[i]`, which doesn't
make any sense.

```
  for (size_t i = 0; i < n; i++) {
    __asm__ volatile("invlpg %0" : : "m"(p1[i]));
    if (p1[i] != p2[i]) {
      return p1[i] < p2[i] ? -1 : 1;
    }
  }

// this works! which doesn't make any sense
```

Furthermore, since we always have this issue for page 0x2008 and
0xC0002008, flushing those TLB entries beforehand should also work,
but the check fails if I do the following.

```
  __asm__ volatile("invlpg 0x00002008\n\t"
                   "invlpg 0xC0002008");
  for (size_t i = 0; i < n; i++) {
    if (p1[i] != p2[i]) {
      return p1[i] < p2[i] ? -1 : 1;
    }
  }

// check fails
```

But the check passes if I do the following, which should be identical
in behavior (but inefficient).

```
  for (size_t i = 0; i < n; i++) {
    __asm__ volatile("invlpg 0x00002008\n\t"
                     "invlpg 0xC0002008");
    if (p1[i] != p2[i]) {
      return p1[i] < p2[i] ? -1 : 1;
    }
  }

// check passes ... why???
```

Actually, putting _any_ address as argument to `invlpg` works when
it's in the loop. It turns out that there's a temporal aspect to this
-- if we call `invlpg` on _any address_ shortly before we reach the
0x2008 address, that's good enough to make the check pass.

```
  for (size_t i = 0; i < n; i++) {
    // Check fails with 0x1FF0 but passes with 0x1FF8-0x2008.
    if ((unsigned)&p1[i] == 0x1FF0) {
      __asm__ volatile("invlpg 0");
    }

    if (p1[i] != p2[i]) {
      return p1[i] < p2[i] ? -1 : 1;
    }
  }

// check may pass or fail based on when we call `invlpg`
```

The behavior seems a bit probabilistic as if there's a race
condition. For example, if we set the check to 0x1FF8, the check
passes roughly half of the time on my machine.

You may notice that I haven't posted any assembly here yet, and the
reason is I haven't found anything conclusive there. I also have no
idea where this magic address 0x2008 comes from. This is lower in
memory than anything installed by the bootloader (lowest memory is the
stack which is just under 0x7000) or in the [standard low memory
map](https://wiki.osdev.org/Memory_Map_(x86)).

I give up at this point. I notice that the check works if I explicitly
read from the 0x2000 memory page beforehand. Hence the following
kludge just after setting up the page table:

```
  // Without this running with `-accel kvm` bugs out. I have no idea
  // why. Without this we get some _really_ weird behavior later on
  // when trying to access address 0x2008/0xC0002008 as if the TLB is
  // out of date. It's a flakey behavior and really annoying to debug
  // (doesn't show up when running in the debugger). Invalidating the
  // mapping from the TLB (via invlpg) doesn't seem to help; the only
  // thing that helps is accessing the memory address before the first
  // time it's accessed. This will probably surface as other bugs in
  // the future, IDK.
  volatile int _ = *(int *)0x2000;
```

This works if I dereference any address in the range `0x1FFD` through
`0x2FFF`, which is an almost-page-oriented behavior. I have no idea
why the last 12 bytes of the 0x1000 page works here, but it seems to
be consistent behavior.

## A fun memset

Overwriting stack memory or the GDT can lead to fun errors! Both of
the following happened on the same `memset()` call, leading to double
the f(r)u(stratio)n!

Clobbering the stack: I'd misplaced it via a bad asm instruction :/
Memory corruption is a fun thing. Led to a page fault within
`memset()` (not the source/destination memory regions, but the text
region :/).

Obliterating the GDT: I marked the bootloader text region reclaimable
by the kernel and forgot the GDT was in there. The GP faults with
error 0x8 (segment-related error) should've been a clue. This only
popped up after the next `iret` since that's the next time a segment
register is loaded from the GDT.
