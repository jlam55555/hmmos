#pragma once

/// \file unistd.h
/// \brief POSIX-like system API. Namely syscalls

#include <cstddef>
#include <cstdint>

using ssize_t = int32_t;

__attribute__((noreturn)) void exit(int status);

ssize_t read(int fd, void *buf, size_t count);
