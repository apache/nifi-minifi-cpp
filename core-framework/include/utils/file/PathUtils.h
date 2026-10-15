/**
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The ASF licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#pragma once

#include <climits>
#include <cctype>
#include <cinttypes>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include "utils/expected.h"

namespace org::apache::nifi::minifi::utils::file {

namespace PathUtils = ::org::apache::nifi::minifi::utils::file;
using path = const char*;

std::string globToRegex(std::string glob);

inline std::optional<std::filesystem::path> canonicalize(const std::filesystem::path& path) {
  std::error_code canonical_error;
  auto result = std::filesystem::canonical(path, canonical_error);
  if (canonical_error)
    return std::nullopt;

  return result;
}

inline std::expected<void, std::string> validateRelativeFilePath(const std::filesystem::path& path) {
  if (path.empty()) {
    return std::unexpected{"Empty file path"};
  }
  if (!path.is_relative()) {
    return std::unexpected{"File path must be a relative path '" + path.string() + "'"};
  }
  if (!path.has_filename()) {
    return std::unexpected{"Filename missing in output path '" + path.string() + "'"};
  }
  if (path.filename() == "." || path.filename() == "..") {
    return std::unexpected{"Invalid filename '" + path.filename().string() + "'"};
  }
  for (const auto& segment : path) {
    if (segment == "..") {
      return std::unexpected{"Accessing parent directory is forbidden in file path '" + path.string() + "'"};
    }
  }
  return {};
}


/**
 * Represents filesystem space information in bytes
 */
struct space_info {
  std::uintmax_t capacity;
  std::uintmax_t free;
  std::uintmax_t available;

  friend bool operator==(const space_info& a, const space_info& b) noexcept {
    return a.capacity == b.capacity && a.free == b.free && a.available == b.available;
  }
};

class filesystem_error : public std::system_error {
 public:
  filesystem_error(const std::string& what_arg, const std::error_code ec)
      :std::system_error{ec, what_arg}
  {}
  filesystem_error(const std::string& what_arg, const path path1, const path path2, const std::error_code ec)
      :std::system_error{ec, what_arg}, paths_involved_{std::make_shared<const std::pair<std::string, std::string>>(path1, path2)}
  {}

  // copy should be noexcept as soon as libstdc++ fixes std::system_error copy
  filesystem_error(const filesystem_error& o) = default;
  filesystem_error& operator=(const filesystem_error&) = default;

  path path1() const noexcept { return paths_involved_->first.c_str(); }
  path path2() const noexcept { return paths_involved_->second.c_str(); }

 private:
  std::shared_ptr<const std::pair<std::string, std::string>> paths_involved_;
};

/**
 * Provides filesystem space information for the specified directory
 */
space_info space(path);
space_info space(path, std::error_code&) noexcept;

/**
 * The grammar used to interpret a path string. POSIX and Windows paths are not merely spelled
 * differently: the same string denotes a different path under each, so "//host/share" is a UNC
 * path on Windows but an ordinary absolute path on POSIX, and a backslash is a separator on
 * Windows but an ordinary filename character on POSIX.
 */
enum class PathFlavor { Posix, Windows };

#ifdef WIN32
constexpr PathFlavor native_path_flavor = PathFlavor::Windows;
#else
constexpr PathFlavor native_path_flavor = PathFlavor::Posix;
#endif

/**
 * Converts an absolute, already-generic (forward-slash separated) path to a file: URI.
 *
 * Kept separate from pathToUri so that every platform-specific rule lives in one flavor-parametric
 * function that can be unit tested on any host: a std::filesystem::path only ever carries the
 * grammar of the platform it was compiled for, so Windows rules would otherwise go unexercised on
 * POSIX CI and vice versa.
 */
std::expected<std::string, std::string> genericPathToUri(std::string_view generic_path, PathFlavor flavor);

/**
 * Converts a native path to a file: URI, resolving relative paths against the current working
 * directory. Does not throw.
 */
std::expected<std::string, std::string> pathToUri(const std::filesystem::path& path);

}  // namespace org::apache::nifi::minifi::utils::file
