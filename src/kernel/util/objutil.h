#pragma once

/// If an object manages some resource, and copying doesn't produce an
/// equivalent result.
#define NON_COPYABLE(Class)                                                    \
  Class(Class &) = delete;                                                     \
  Class &operator=(Class &) = delete;

/// If an object needs to have a stable reference, e.g., if a pointer
/// is stored in some container. E.g., any object containing an
/// intrusive list should be marked NON_MOVABLE.
#define NON_MOVABLE(Class)                                                     \
  NON_COPYABLE(Class)                                                          \
  Class(Class &&) = delete;                                                    \
  Class &operator=(Class &&) = delete;
