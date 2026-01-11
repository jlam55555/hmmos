# userspace

## compilation strategy

Keep it simple:
- jlibc (compiled separately)
- everything else: single file or single directory compilation
- everything specified in one userspace makefile
- statically compiled binaries, no /lib or /include necessary

## dependencies

In order to get a simple shell (/bin/josh) up and running, we need the
following interfaces:

- [ ] syscalls
    - [x] exit
    - [ ] read
    - [ ] write
    - [ ] fork/clone
    - [ ] exec
- [ ] simple jlibc syscall wrappers
- [ ] a simple binary (e.g., /bin/cat)
- [ ] terminal driver

TODO: At the time of writing, read/write are pretty straightforward
wrappers down to the inode/page cache implementations. fork/clone/exec
just require some attention to detail. It also requires the teardown
of process resources (cleaning up the page table mapping, flushing
files etc.). The terminal driver is its own effort, but we can start
by having the shell read input from a file rather than stdin.
