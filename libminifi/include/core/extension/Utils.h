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

#include "minifi-c/minifi-c.h"
#include "minifi-cpp/utils/gsl.h"
#include "utils/StringUtils.h"
#include "utils/file/FileUtils.h"
#include "utils/file/PathUtils.h"
#include "utils/RegexUtils.h"
#include "minifi-c/minifi-c.h"

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

enum LibraryType {
  Cpp,
  CApi,
  Invalid
};

struct LibraryDescriptor {
  std::string name;
  std::filesystem::path dir;
  std::string filename;


  [[nodiscard]] bool verify_as_cpp_extension() const {
    const auto path = getFullPath();
    if (!std::filesystem::exists(path)) {
      throw std::runtime_error{"File not found: " + path.string()};
    }
    const std::string_view begin_marker = "__EXTENSION_BUILD_IDENTIFIER_BEGIN__";
    const std::string_view end_marker = "__EXTENSION_BUILD_IDENTIFIER_END__";
    const std::string magic_constant = utils::string::join_pack(begin_marker, AgentBuild::BUILD_IDENTIFIER, end_marker);
    return utils::file::contains(path, magic_constant);
  }

  [[nodiscard]] bool verify_as_c_extension(const std::shared_ptr<logging::Logger>& logger) const {
    const auto path = getFullPath();
    if (!std::filesystem::exists(path)) {
      throw std::runtime_error{"File not found: " + path.string()};
    }
    const std::string_view api_tag_prefix = "MINIFI_API_VERSION=[";
    if (auto version = utils::file::find(path, api_tag_prefix, api_tag_prefix.size() + 20)) {
      utils::SVMatch match;
      if (!utils::regexSearch(version.value(), match, utils::Regex{"MINIFI_API_VERSION=\\[([0-9]+)\\.([0-9]+)\\.([0-9]+)\\]"})) {
        logger->log_error("Found api version in invalid format: '{}'", version.value());
        return false;
      }
      gsl_Assert(match.size() == 4);
      const int major = std::stoi(match[1]);
      const int minor = std::stoi(match[2]);
      const int patch = std::stoi(match[3]);
      if (major != MINIFI_API_MAJOR_VERSION) {
        logger->log_error("API major version mismatch, application is '{}' while extension is '{}.{}.{}'", MINIFI_API_VERSION, major, minor, patch);
        return false;
      }
      if (minor > MINIFI_API_MINOR_VERSION) {
        logger->log_error("Extension is built for a newer version, application is '{}' while extension is '{}.{}.{}'", MINIFI_API_VERSION, major, minor, patch);
        return false;
      }
      return true;
    }
    return false;
  }

  [[nodiscard]]
  LibraryType verify(const std::shared_ptr<logging::Logger>& logger) const {
    const auto path = getFullPath();
    const Timer timer{[&](const std::chrono::milliseconds elapsed) {
      logger->log_debug("Verification for '{}' took {}", path, elapsed);
    }};
    if (verify_as_cpp_extension()) {
      return Cpp;
    }
    if (verify_as_c_extension(logger)) {
      return CApi;
    }
    return Invalid;
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
