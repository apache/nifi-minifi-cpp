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

#include <optional>
#include <functional>
#include <chrono>
#include "utils/gsl.h"
#include "utils/file/FileView.h"
#include "utils/StringUtils.h"

namespace org::apache::nifi::minifi::core::extension::internal {

struct Timer {
  explicit Timer(std::function<void(int)> cb): cb_(std::move(cb)) {}
  ~Timer() {
    auto end = std::chrono::steady_clock::now();
    int elapsed = gsl::narrow<int>(std::chrono::duration_cast<std::chrono::milliseconds>(end - start_).count());
    cb_(elapsed);
  }
  std::chrono::steady_clock::time_point start_{std::chrono::steady_clock::now()};
  std::function<void(int)> cb_;
};

struct LibraryDescriptor {
  std::string name;
  std::filesystem::path dir;
  std::string filename;

  [[nodiscard]]
  bool verify(const std::shared_ptr<logging::Logger>& logger) const {
    auto path = getFullPath();
    Timer timer{[&] (int ms) {
      logger->log_error("Verification for '%s' took %d ms", path.string(), ms);
    }};
    try {
      utils::file::FileView file(path);
      const std::string_view begin_marker = "__EXTENSION_BUILD_IDENTIFIER_BEGIN__";
      const std::string_view end_marker = "__EXTENSION_BUILD_IDENTIFIER_END__";
      auto build_id_begin = std::search(file.begin(), file.end(), begin_marker.begin(), begin_marker.end());
      if (build_id_begin == file.end()) {
        logger->log_error("Couldn't find start of build identifier in '%s'", path.string());
        return false;
      }
      std::advance(build_id_begin, begin_marker.length());
      auto build_id_end = std::search(build_id_begin, file.end(), end_marker.begin(), end_marker.end());
      if (build_id_end == file.end()) {
        logger->log_error("Couldn't find end of build identifier in '%s'", path.string());
        return false;
      }
      std::string build_id(build_id_begin, build_id_end);
      if (build_id != AgentBuild::BUILD_IDENTIFIER) {
        logger->log_error("Build identifier does not match in '%s', expected '%s', got '%s'", path.string(), AgentBuild::BUILD_IDENTIFIER, build_id);
        return false;
      }
    } catch (const utils::file::FileView::Failure& file_error) {
      logger->log_error("Error while verifying library '%s': %s", path.string(), file_error.what());
      return false;
    }
    return true;
  }

  [[nodiscard]]
  std::filesystem::path getFullPath() const {
    return dir / filename;
  }
};

std::optional<LibraryDescriptor> asDynamicLibrary(const std::filesystem::path& path) {
#if defined(WIN32)
  const std::string extension = ".dll";
#elif defined(__APPLE__)
  const std::string extension = ".dylib";
#else
  const std::string extension = ".so";
#endif

#ifdef WIN32
  const std::string prefix = "";
#else
  const std::string prefix = "lib";
#endif
  std::string filename = path.filename().string();
  if (!utils::StringUtils::startsWith(filename, prefix) || !utils::StringUtils::endsWith(filename, extension)) {
    return {};
  }
  return LibraryDescriptor{
      filename.substr(prefix.length(), filename.length() - extension.length() - prefix.length()),
      path.parent_path(),
      filename
  };
}

}  // org::apache::nifi::minifi::core::extension::internal
