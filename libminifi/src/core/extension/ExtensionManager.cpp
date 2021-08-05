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
#include "core/logging/LoggerConfiguration.h"
#include "utils/file/FileUtils.h"
#include "core/extension/Executable.h"
#include "utils/file/FileMatcher.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace core {
namespace extension {

namespace {
struct LibraryDescriptor {
  std::string name;
  std::string dir;
  std::string filename;

  bool verify(const std::shared_ptr<logging::Logger>& /*logger*/) const {
    // TODO(adebreceni): check signature
    return true;
  }

  std::string getFullPath() const {
    return utils::file::PathUtils::concat_path(dir, filename);
  }
};
}  // namespace

static std::optional<LibraryDescriptor> asDynamicLibrary(const std::string& dir, const std::string& filename) {
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
  if (!utils::StringUtils::startsWith(filename, prefix) || !utils::StringUtils::endsWith(filename, extension)) {
    return {};
  }
  return LibraryDescriptor{
    filename.substr(prefix.length(), filename.length() - extension.length() - prefix.length()),
    dir,
    filename
  };
}

std::shared_ptr<logging::Logger> ExtensionManager::logger_ = logging::LoggerFactory<ExtensionManager>::getLogger();

ExtensionManager::ExtensionManager() {
  modules_.push_back(std::make_unique<Executable>());
  active_module_ = modules_[0].get();
}

ExtensionManager& ExtensionManager::get() {
  static ExtensionManager instance;
  return instance;
}

bool ExtensionManager::initialize(const std::shared_ptr<Configure>& config) {
  static bool initialized = ([&] {
    logger_->log_trace("Initializing extensions");
    // initialize executable
    active_module_->initialize(config);
    std::optional<std::string> pattern = config ? config->get(nifi_extension_path) : std::nullopt;
    if (!pattern) return;
    std::vector<LibraryDescriptor> libraries;
    utils::file::FileMatcher(pattern.value()).forEachFile([&] (const std::string& dir, const std::string& filename) {
      std::optional<LibraryDescriptor> library = asDynamicLibrary(dir, filename);
      if (library && library->verify(logger_)) {
        libraries.push_back(std::move(library.value()));
      }
      return true;
    });
    for (const auto& library : libraries) {
      auto module = std::make_unique<DynamicLibrary>(library.name, library.getFullPath());
      active_module_ = module.get();
      if (!module->load()) {
        // error already logged by method
        continue;
      }
      if (!module->initialize(config)) {
        logger_->log_error("Failed to initialize module '%s' at '%s'", library.name, library.getFullPath());
      } else {
        modules_.push_back(std::move(module));
      }
    }
  }(), true);
  return initialized;
}

void ExtensionManager::registerExtension(Extension *extension) {
  active_module_->registerExtension(extension);
}

void ExtensionManager::unregisterExtension(Extension *extension) {
  for (const auto& module : modules_) {
    if (module->unregisterExtension(extension)) {
      return;
    }
  }
}

}  // namespace extension
}  // namespace core
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
