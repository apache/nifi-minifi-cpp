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

#include <algorithm>
#include <fstream>
#include <functional>
#include <iterator>
#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "minifi-cpp/utils/gsl.h"
#include "utils/StringUtils.h"
#include "utils/file/FileUtils.h"
#include "utils/file/PathUtils.h"

namespace org::apache::nifi::minifi::core::extension::internal {

template<typename Callback>
class Timer {
 public:
  explicit Timer(Callback cb): cb_(std::move(cb)) {}
  ~Timer() {
    cb_(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start_));
  }
 private:
  std::chrono::steady_clock::time_point start_{std::chrono::steady_clock::now()};
  Callback cb_;
};

struct LibraryDescriptor {
  std::string name;
  std::filesystem::path dir;
  std::string filename;

  [[nodiscard]]
  bool verify(const std::shared_ptr<logging::Logger>& logger) const {
    const auto path = getFullPath();
    if (!std::filesystem::exists(path)) {
      throw std::runtime_error{"File not found: " + path.string()};
    }
    const Timer timer{[&](const std::chrono::milliseconds elapsed) {
      logger->log_debug("Verification for '{}' took {}", path, elapsed);
    }};
    const std::string_view begin_marker = "__EXTENSION_BUILD_IDENTIFIER_BEGIN__";
    const std::string_view end_marker = "__EXTENSION_BUILD_IDENTIFIER_END__";
    const std::string magic_constant = utils::string::join_pack(begin_marker, AgentBuild::BUILD_IDENTIFIER, end_marker);
    return utils::file::contains(path, magic_constant);
  }

  [[nodiscard]]
  std::filesystem::path getFullPath() const {
    return dir / filename;
  }
};

std::optional<LibraryDescriptor> asDynamicLibrary(const std::filesystem::path& path) {
#if defined(WIN32)
  static const std::string_view extension = ".dll";
#elif defined(__APPLE__)
  static const std::string_view extension = ".dylib";
#else
  static const std::string_view extension = ".so";
#endif

#ifdef WIN32
  static const std::string_view prefix = "";
#else
  static const std::string_view prefix = "lib";
#endif
  if (!path.filename().string().starts_with(prefix) || path.filename().extension().string() != extension) {
    return {};
  }
  std::string filename = path.filename().string();
  return LibraryDescriptor{
      filename.substr(prefix.length(), filename.length() - extension.length() - prefix.length()),
      path.parent_path(),
      filename
  };
}

}  // namespace org::apache::nifi::minifi::core::extension::internal
