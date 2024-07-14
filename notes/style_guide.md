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

## C++ naming convention
C style is pretty simple; we'll use underscore_case for all regular
symbols, and CAPITAL\_CASED macros. (This also goes for `extern "C"`
compatibility C++ code.)

In C++ this gets a little more complicated. For now I'll try to keep
underscore case for filenames and most symbols except datatypes
(PascalCase), but there's a chance I may change my mind about this in
the future. (This is similar to our Python casing conventions at work,
interestingly.)

## Other C++ shenanigans
C++ encapsulation should be fairly open for testing
purposes. Implementation details should usually be put into a `detail`
sub-namespace rather than an anonymous namespace.
