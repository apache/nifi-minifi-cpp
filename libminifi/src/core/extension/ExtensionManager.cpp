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
#include "core/extension/Executable.h"
#include "utils/file/FilePattern.h"
#include "core/extension/DynamicLibrary.h"
#include "agent/agent_version.h"
#include "core/extension/Utils.h"
#include "properties/Configuration.h"
#include "utils/Locations.h"

namespace org::apache::nifi::minifi::core::extension {

const std::shared_ptr<logging::Logger> ExtensionManagerImpl::logger_ = logging::LoggerFactory<ExtensionManager>::getLogger();

ExtensionManagerImpl::ExtensionManagerImpl()
    : modules_([] {
        std::vector<std::unique_ptr<Module>> modules;
        modules.push_back(std::make_unique<Executable>());
        return modules;
      }()),
      active_module_(modules_[0].get()) {
}

ExtensionManagerImpl& ExtensionManagerImpl::get() {
  static ExtensionManagerImpl instance;
  return instance;
}

ExtensionManager& ExtensionManager::get() {
  return ExtensionManagerImpl::get();
}

bool ExtensionManagerImpl::initialize(const std::shared_ptr<Configure>& config) {
  static bool initialized = ([&] {
    logger_->log_trace("Initializing extensions");
    // initialize executable
    active_module_->initialize(config);
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

      auto module = std::make_unique<DynamicLibrary>(library->name, library->getFullPath());
      active_module_ = module.get();
      if (!module->load()) {
        // error already logged by method
        continue;
      }
      // some modules (shared libraries) might need to be loaded into the global namespace exposing
      // their definitions (e.g. python script extension)
      if (module->findSymbol("LOAD_MODULE_AS_GLOBAL")) {
        // reload library as global
        if (!module->load(true)) {
          continue;
        }
      }
      if (!module->initialize(config)) {
        logger_->log_error("Failed to initialize module '{}' at '{}'", library->name, library->getFullPath());
      } else {
        logger_->log_trace("Successfully initialized extension '{}' at '{}'", library->name, library->getFullPath());
        modules_.push_back(std::move(module));
      }
    }
  }(), true);
  return initialized;
}

void ExtensionManagerImpl::registerExtension(Extension& extension) {
  active_module_->registerExtension(extension);
}

void ExtensionManagerImpl::unregisterExtension(Extension& extension) {
  for (const auto& module : modules_) {
    if (module->unregisterExtension(extension)) {
      return;
    }
  }
}

}  // namespace org::apache::nifi::minifi::core::extension
