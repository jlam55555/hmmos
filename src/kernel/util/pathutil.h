#pragma once

#include "nonstd/string_view.h"

namespace util::path {

/// Returns {dirname, basename}.
std::pair<nonstd::string_view, nonstd::string_view>
partition_path(nonstd::string_view path);

inline nonstd::string_view dirname(nonstd::string_view path) {
  return partition_path(path).first;
}
inline nonstd::string_view basename(nonstd::string_view path) {
  return partition_path(path).second;
}

/// Returns {first_component, remaining_path}. Useful for traversing
/// path from the root.
std::pair<nonstd::string_view, nonstd::string_view>
left_partition_path(nonstd::string_view path);

} // namespace util::path
