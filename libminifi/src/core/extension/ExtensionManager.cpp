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

#include "core/extension/ExtensionManager.h"

#include <algorithm>

#include "core/logging/LoggerConfiguration.h"
#include "core/extension/Extension.h"
#include "minifi-cpp/agent/agent_docs.h"
#include "utils/file/FilePattern.h"
#include "minifi-cpp/agent/agent_version.h"
#include "core/extension/Utils.h"
#include "properties/Configuration.h"
#include "utils/Locations.h"

namespace org::apache::nifi::minifi::core::extension {

ExtensionManager::ExtensionManager(const std::shared_ptr<Configure>& config): logger_(logging::LoggerFactory<ExtensionManager>::getLogger()) {
  logger_->log_trace("Initializing extensions");
  if (!config) {
    logger_->log_error("Missing configuration");
    return;
  }
  const std::string pattern = [&] {
    /**
     * Comma separated list of path patterns. Patterns prepended with "!" result in the exclusion
     * of the extensions matching that pattern, unless some subsequent pattern re-enables it.
     */
    if (const auto opt_pattern = config->get(minifi::Configuration::nifi_extension_path)) {
      return *opt_pattern;
    };

    auto default_extension_path = utils::getDefaultExtensionsPattern();
    logger_->log_warn("No extension path is provided in properties, using default: '{}'", default_extension_path);
    return std::string(default_extension_path);
  }();

  auto candidates = utils::file::match(utils::file::FilePattern(pattern, [&] (std::string_view subpattern, std::string_view error_msg) {
    logger_->log_error("Error in subpattern '{}': {}", subpattern, error_msg);
  }));
  for (const auto& candidate : candidates) {
    auto library = internal::asDynamicLibrary(candidate);
    if (!library) {
      continue;
    }
    const auto library_type = library->verify(logger_);
    if (library_type == internal::Invalid) {
      logger_->log_warn("Skipping library '{}' at '{}': failed verification, different build?",
          library->name, library->getFullPath());
      continue;
    }

    logger_->log_trace("Verified library {} at {} as {} extension", library->name, library->getFullPath(), magic_enum::enum_name(library_type));
    auto extension = std::make_unique<Extension>(library->name, library->getFullPath());
    if (!extension->load()) {
      // error already logged by method
      continue;
    }
    // some shared libraries might need to be loaded into the global namespace exposing
    // their definitions (e.g. python script extension)
    if (extension->findSymbol("LOAD_MODULE_AS_GLOBAL")) {
      // reload library as global
      if (!extension->load(true)) {
        continue;
      }
    }
    if (!extension->initialize(config)) {
      logger_->log_error("Failed to initialize extension '{}' at '{}'", library->name, library->getFullPath());
    } else {
      extensions_.push_back(std::move(extension));
    }
  }
}

}  // namespace org::apache::nifi::minifi::core::extension
