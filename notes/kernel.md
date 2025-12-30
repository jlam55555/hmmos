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
- [X] basics
	- [X] printf library
	- [X] (circular) linked lists like Linux's LIST_HEAD
- [X] interrupts
- [ ] memory allocation
	- [X] page frame allocator
	- [ ] slub/slab allocator
	- [X] virtual memory allocator
- [X] scheduling
	- [X] kernel threads
	- [X] setup TSS (for userspace)
	- [X] userspace processes (after userspace)
- [ ] enter userspace
	- [X] syscalls
	- [ ] shell
	- [X] ELF loader
	- [ ] try to link in a very simple libc?
- [ ] drivers
	- [X] serial port
	- [ ] keyboard
	- [ ] tty
	- [X] console
	- [X] disk
	- [ ] timer
	- [ ] graphics (very simple)
