# demos.md
Educational demonstrations of important OS behavior

### locking
The very basic example: two threads incrementing a shared
variable. There are some notable omissions here but this is to get the
basic idea.

```
constexpr uint64_t n = 1'000'000'000;
uint64_t j = 0;
uint64_t done1 = 0;

void foo() {
  for (uint64_t i = 0; i < n; ++i) {
    ++j;
  }

  done1 = arch::time::rdtsc();
  scheduler->destroy_thread();
}

void main() {
  sched::Scheduler scheduler;
  scheduler.bootstrap();
  scheduler.new_thread(&foo);

  uint64_t start = arch::time::rdtsc();
  for (uint64_t i = 0; i < n; ++i) {
    ++j;
  }
  uint64_t done2 = arch::time::rdtsc();

  while (!done1) {
    hlt;
  }

  nonstd::printf("j=%llu\r\n", j);
  nonstd::printf("elapsed=%llu\r\n", std::max(done1, done2) - start);
}
```

Interestingly, QEMU emulation by default generates an atomic increment
operation, but with KVM we see the intended effect. We can introduce
locking in a uniprocessor system using `cli`/`sti` around the
"critical sections" (the increment ops).

```
j=2000000000  # QEMU default
j=1816044630  # KVM
j=2000000000  # KVM w/ locking
```

It's also worth measuring the overhead of the locking.

```
elapsed=34767593327   # QEMU, no locking
elapsed=104330302146  # QEMU, locking (200% increase)
elapsed=5601347345    # KVM, no locking
elapsed=16394652151   # KVM, locking (193% increase)
```

Both for QEMU and KVM emulation, it takes almost exactly three times
as long to run with locking enabled vs. no locking.

(Also interesting, the KVM virtualization runs 6.2 times as fast as
the QEMU emulation).

For context, here's the disassembly of the entirety of `foo()`
(compiled with clang with default optimization):

```

fe005e40 <(anonymous namespace)::foo()>:
fe005e40:       55                      push   %ebp
fe005e41:       89 e5                   mov    %esp,%ebp
fe005e43:       56                      push   %esi
fe005e44:       83 ec 14                sub    $0x14,%esp
fe005e47:       c7 45 f4 00 00 00 00    movl   $0x0,-0xc(%ebp)
fe005e4e:       c7 45 f0 00 00 00 00    movl   $0x0,-0x10(%ebp)

# Loop start
fe005e55:       8b 75 f0                mov    -0x10(%ebp),%esi
fe005e58:       8b 4d f4                mov    -0xc(%ebp),%ecx
fe005e5b:       31 c0                   xor    %eax,%eax
fe005e5d:       ba ff c9 9a 3b          mov    $0x3b9ac9ff,%edx
fe005e62:       29 f2                   sub    %esi,%edx
fe005e64:       19 c8                   sbb    %ecx,%eax
fe005e66:       0f 82 2d 00 00 00       jb     fe005e99 <(anonymous namespace)::foo()+0x59>
fe005e6c:       e9 00 00 00 00          jmp    fe005e71 <(anonymous namespace)::foo()+0x31>
fe005e71:       fa                      cli
fe005e72:       a1 30 ca 00 fe          mov    0xfe00ca30,%eax
fe005e77:       83 c0 01                add    $0x1,%eax
fe005e7a:       83 15 34 ca 00 fe 00    adcl   $0x0,0xfe00ca34
fe005e81:       a3 30 ca 00 fe          mov    %eax,0xfe00ca30
fe005e86:       fb                      sti
fe005e87:       8b 45 f4                mov    -0xc(%ebp),%eax
fe005e8a:       83 45 f0 01             addl   $0x1,-0x10(%ebp)
fe005e8e:       83 d0 00                adc    $0x0,%eax
fe005e91:       89 45 f4                mov    %eax,-0xc(%ebp)
fe005e94:       e9 bc ff ff ff          jmp    fe005e55 <(anonymous namespace)::foo()+0x15>
# Loop end

fe005e99:       e8 42 b1 ff ff          call   fe000fe0 <arch::time::rdtsc()>
fe005e9e:       89 15 3c ca 00 fe       mov    %edx,0xfe00ca3c
fe005ea4:       a3 38 ca 00 fe          mov    %eax,0xfe00ca38
fe005ea9:       a1 28 ca 00 fe          mov    0xfe00ca28,%eax
fe005eae:       31 c9                   xor    %ecx,%ecx
fe005eb0:       89 04 24                mov    %eax,(%esp)
fe005eb3:       c7 44 24 04 00 00 00    movl   $0x0,0x4(%esp)
fe005eba:       00
fe005ebb:       e8 90 01 00 00          call   fe006050 <sched::Scheduler::destroy_thread(sched::KernelThread*)>
fe005ec0:       83 c4 14                add    $0x14,%esp
fe005ec3:       5e                      pop    %esi
fe005ec4:       5d                      pop    %ebp
fe005ec5:       c3                      ret
```

So while one might be naively tempted to say that `cli`/`sti` add two
"instructions" (line of code) to the loop body hence increasing the
runtime by 300%, this is clearly not true by looking at the
disassembled loop body.
