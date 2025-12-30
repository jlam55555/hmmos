#pragma once
/// \file unistd.h

#include <cstddef>
#include <cstdint>

using ssize_t = int32_t;

ssize_t read(int fd, void *buf, size_t count);
