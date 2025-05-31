#include "util/pathutil.h"

namespace util::path {

std::pair<nonstd::string_view, nonstd::string_view>
partition_path(nonstd::string_view path) {
  const auto sep = path.rfind('/');
  return sep != nonstd::string_view::npos
             ? std::pair{path.substr(0, sep), path.substr(sep + 1)}
             : std::pair{"", path};
}

std::pair<nonstd::string_view, nonstd::string_view>
left_partition_path(nonstd::string_view path) {
  const auto sep = path.find('/');
  return sep != nonstd::string_view::npos
             ? std::pair{path.substr(0, sep), path.substr(sep + 1)}
             : std::pair{path, ""};
}

} // namespace util::path
