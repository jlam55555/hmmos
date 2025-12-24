#pragma once

/// \file result.h
/// \brief \ref Result enum, a two-byte enum akin to errno.
///
/// This lives in namespace fs since most use cases have to do with
/// the VFS. This enum is intended to be used for all syscalls (i.e.,
/// for communicating between kernel and userspace).

#include <cstdint>

namespace fs {

enum class Result : uint16_t {
  Ok = 0,

  // Generic reasons
  Unsupported,
  InvalidArgs,

  // VFS
  IsDirectory,
  IsFile,
  FileNotFound,
  BadFD,

  // exec/fork
  NonExecutable,

  // mmap
  MappingExists,
};

inline const char *result_to_str(Result res) {
  switch (res) {
#define X(ENUM)                                                                \
  case Result::ENUM:                                                           \
    return #ENUM
    X(Ok);
    X(Unsupported);
    X(InvalidArgs);
    X(IsDirectory);
    X(IsFile);
    X(FileNotFound);
    X(BadFD);
    X(NonExecutable);
    X(MappingExists);
#undef X
  }
  return "<unknown>";
}

} // namespace fs
