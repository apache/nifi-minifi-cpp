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

#include "core/extension/Utils.h"
#include "minifi-c/minifi-c.h"
#include "minifi-cpp/agent/agent_version.h"
#include "utils/RegexUtils.h"

namespace org::apache::nifi::minifi::core::extension::internal {

[[nodiscard]] bool LibraryDescriptor::verify_as_cpp_extension() const {
  const auto path = getFullPath();
  if (!std::filesystem::exists(path)) {
    throw std::runtime_error{"File not found: " + path.string()};
  }
  const std::string_view begin_marker = "__EXTENSION_BUILD_IDENTIFIER_BEGIN__";
  const std::string_view end_marker = "__EXTENSION_BUILD_IDENTIFIER_END__";
  const std::string magic_constant = utils::string::join_pack(begin_marker, AgentBuild::BUILD_IDENTIFIER, end_marker);
  return utils::file::contains(path, magic_constant);
}

[[nodiscard]] bool LibraryDescriptor::verify_as_c_extension(const std::shared_ptr<logging::Logger>& logger) const {
  const auto path = getFullPath();
  if (!std::filesystem::exists(path)) {
    throw std::runtime_error{"File not found: " + path.string()};
  }
  const std::string_view api_tag_prefix = "MINIFI_API_VERSION=[";
  if (auto version = utils::file::find(path, api_tag_prefix, api_tag_prefix.size() + 20)) {
    utils::SVMatch match;
    if (!utils::regexSearch(version.value(), match, utils::Regex{R"(MINIFI_API_VERSION=\[([0-9]+)\.([0-9]+)\.([0-9]+)\])"})) {
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

}  // namespace org::apache::nifi::minifi::core::extension::internal
