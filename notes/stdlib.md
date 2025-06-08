# stdlib.md
Notes about compliance with the standard C++ library (libstdc++ in
most cases).

The general guideline is header-only symbols are allowable due to
`-nostdlib`.

## Concepts and type traits
Always usable.

## Types
Scalar typedefs (`std::size_t`, `std::uintptr_t`, etc.) and
allocation-/policy-free types (`std::pair`, `std::array`,
`std::initializer_list`, etc.) are fine. The standard doesn't define
that these types don't allocate, but any reasonable implementation
won't.

Types that throw are generally unsupported. These generally call into
hooks like `__throw_length_error()` that are defined in object
files. We _could_ define these stubs (to abort rather than throw --
see libc_symbols.cc for examples), but in general this practice is not
portable since these are not defined in the standard. The
`std::allocator` class is a prominent example, as well as many
dynamic-allocation container classes (discussed below).

Similarly, I try to stay away from types that require complicated
policy classes or function hooks. Both for simplicity and for the
learning experience, I opt to implement these classes myself. For
example, for `std::unordered_map` requires you to implement
`std::hash` for key types (this is unavoidable), as well as hashing
policy methods such as
`std::__detail::_Prime_rehash_policy::_M_next_bucket(unsigned)` and
`std::__detail::_Prime_rehash_policy::_M_need_rehash(unsigned,
unsigned, unsigned) const`. For this reason I've decided to provide
simple implementations of the major allocating container types
(`list`, `vector`, `string`, `unordered_map`, `deque`, etc.) that we
care about.

These guidelines are not strict. For example, `std::string_view` is a
non-allocating header-only type, but I defined a simple implementation
(1) for the educational value; (2) to use C++23
`std::string_view::contains` with a non-C++23 compiler; (3) to
simplify by not templating on `CharT` and depending on
`std::char_traits<CharT>`, since we don't care about wide chars in the
kernel. Another exemption is `std::function`, which can throw
(`__throw_bad_function_call()`), but we use it anyways because we can
use the rest of it as-is.

### Self-implemented stdlib containers
- `string_view`
- `string`
- `vector`
- `node_hash_map` (like `std::unordered_map`)
- TODO: `node_hash_set` (like `std::unordered_set`)
- TODO: `flat_hash_map` (like `absl::flat_hash_map`)
- TODO: `flat_hash_set` (like `absl::flat_hash_set`)
- TODO: `map`
- TODO: `set`
- TODO: `forward_list`
- TODO: `deque` (and `stack`, `queue`)

## Algorithms
Header-only libraries (that don't throw) generally can be used. This
includes most templated algorithms, such as `std::max`, `std::swap`,
and even `std::sort`. I haven't encountered issues with stdlib
algorithm functions requiring symbols defined in object files.
