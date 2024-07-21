#pragma once
#include <cassert>

/// Runtime assertion macros. Note that `static_assert` will remain as
/// it is.

/// This is to make `assert()` more similar to the DEBUG_ASSERT and
/// TEST_ASSERT macros.
///
/// The extra pair of parens is due to `assert` failing to accept an
/// argument with commas.
#define ASSERT(...) assert((__VA_ARGS__))

/// Compile-out debug macro in non-debug builds.
#ifdef DEBUG
#define DEBUG_ASSERT assert
#else
#define DEBUG_ASSERT(...)
#endif
