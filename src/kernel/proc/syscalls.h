#pragma once

/// \file syscalls.h
/// \brief Syscall enum

#include <cstdint>

namespace proc {

enum class Syscall : uint16_t {
  Exit = 1,
};

}
