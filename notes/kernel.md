# overall kernel implementation notes

## kernel TODO
In no definitive order. Some of these can be copied from
[laos](https://github.com/jlam55555/laos)

- [ ] basics
	- [X] printf library
	- [ ] (circular) linked lists like Linux's LIST_HEAD
- [ ] interrupts
- [ ] memory allocation
	- [ ] page frame allocator
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
- [ ] quality of life
	- [X] C++
	- [ ] unit testing
	- [ ] clang-tidy and clang-format configuration
- [ ] drivers
	- [ ] serial port
	- [ ] keyboard
	- [ ] console
	- [ ] disk
	- [ ] timer
	- [ ] graphics (very simple)
- [ ] filesystem (I have no idea how these work)
