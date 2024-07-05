# style guide

## Assembly
As noted in random.md, I'm not using an asm formatter. The formatting
is pretty manual. Directives, comments, and instructions are indented
with a single tab character, labels are not.

## C structs
Reserved (must be zero) and ignored/available (don't care) (bit)fields
are explicitly named with `rsv<n>` or `ign<n>`, where `<n>` is a
number to make such fields unique. As a result, we shouldn't have any
anonymous bitfields.

## Line length
For most things (c code, asm code, markdown, git commit summaries),
I'm using emacs' default `fill-line` (`M-q`) logic, which breaks lines
to roughly 70 chars. clang also breaks long lines to 80 chars.
