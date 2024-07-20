#pragma once
#include <cassert>

/// Runtime assertion macros. Note that `static_assert` will remain as
/// it is.

/// This is to make `assert()` more similar to the DEBUG_ASSERT and
/// TEST_ASSERT macros.
#define ASSERT assert

/// Compile-out debug macro in non-debug builds.
#ifdef DEBUG
#define DEBUG_ASSERT assert
#else
#define DEBUG_ASSERT(...)
#endif
