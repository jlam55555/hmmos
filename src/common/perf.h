#pragma once

/// \file
/// \brief Utilities for performance optimization

#define likely(x) __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)
