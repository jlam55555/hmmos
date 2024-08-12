# overall kernel implementation notes

## Testing setup
The approach I took in laos was not so good, so I'd like to
reformulate it here. There, you would compile the kernel with
`-DRUNTEST=<test_selection_criteria`, and this would only compile in
the tests you want. This means that running a different set of tests
means recompiling the kernel.

There's no reason this can't be done at runtime, so the new idea is to
build a test runner kernel that reads the test selection at runtime
(via serial port).

## kernel TODO
In no definitive order. Some of these can be copied from
[laos](https://github.com/jlam55555/laos).

- [ ] quality of life
	- [X] C++ basic runtime support (e.g., global ctors)
	- [X] unit testing utility
	- [X] doxygen
	- [ ] benchmarking utility
	- [ ] clang-tidy and clang-format configuration
- [ ] basics
	- [X] printf library
	- [X] (circular) linked lists like Linux's LIST_HEAD
- [X] interrupts
- [ ] memory allocation
	- [X] page frame allocator
	- [ ] slub/slab allocator
	- [ ] virtual memory allocator
- [ ] scheduling
	- [ ] kernel threads
	- [ ] setup TSS (for userspace)
	- [ ] userspace processes (after userspace)
- [ ] enter userspace
	- [ ] syscalls
	- [ ] shell
	- [ ] ELF loader
	- [ ] try to link in a very simple libc?
- [ ] drivers
	- [ ] serial port
	- [ ] keyboard
	- [ ] console
	- [ ] disk
	- [ ] timer
	- [ ] graphics (very simple)
- [ ] filesystem (I have no idea how these work)
