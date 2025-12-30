userspace compilation strategy

basically:
- jlibc (compiled separately)
- everything else: single file or single directory compilation
- everything specified in one makefile
- simple enough

what do we do with include paths?
just need a separate .clangd for this
c or cpp?

build everything into /bin

no need for /lib and /include, since we don't have shared libraries

NOCOMMIT working here

https://en.wikipedia.org/wiki/C_standard_library
