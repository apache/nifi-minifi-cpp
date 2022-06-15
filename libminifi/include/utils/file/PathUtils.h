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
#include <memory>
#include <optional>
#include <string>
#include <system_error>
#include <utility>

namespace org::apache::nifi::minifi::utils::file {

namespace PathUtils = ::org::apache::nifi::minifi::utils::file;
using path = const char*;

/**
 * Extracts the filename and path performing some validation of the path and output to ensure
 * we don't provide invalid results.
 * @param path input path
 * @param filePath output file path
 * @param fileName output file name
 * @return result of the operation.
 */
bool getFileNameAndPath(const std::string &path, std::string &filePath, std::string &fileName);

/**
 * Resolves the supplied path to an absolute pathname using the native OS functions
 * (realpath(3) on *nix, GetFullPathNameA on Windows)
 * @param path the name of the file
 * @return the canonicalized absolute pathname on success, empty string on failure
 */
std::string getFullPath(const std::string& path);

std::string globToRegex(std::string glob);

inline bool isAbsolutePath(const char* const path) noexcept {
#ifdef _WIN32
  return path && std::isalpha(path[0]) && path[1] == ':' && (path[2] == '\\' || path[2] == '/');
#else
  return path && path[0] == '/';
#endif
}

inline std::optional<std::string> canonicalize(const std::string &path) {
  const char *resolved = nullptr;
#ifndef WIN32
  char full_path[PATH_MAX];
  resolved = realpath(path.c_str(), full_path);
#else
  resolved = path.c_str();
#endif

  if (resolved == nullptr) {
    return std::nullopt;
  }
  return std::string(path);
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

}  // namespace org::apache::nifi::minifi::utils::file
