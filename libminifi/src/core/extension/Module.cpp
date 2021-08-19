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

#include <memory>
#include <string>

#include "core/logging/LoggerConfiguration.h"
#include "utils/gsl.h"
#include "properties/Configure.h"
#include "core/extension/Module.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace core {
namespace extension {

std::shared_ptr<logging::Logger> Module::logger_ = logging::LoggerFactory<Module>::getLogger();

Module::Module(std::string name): name_(name) {
  logger_->log_trace("Creating module '%s'", name_);
}

Module::~Module() {
  logger_->log_trace("Destroying module '%s'", name_);
}

std::string Module::getName() const {
  return name_;
}

void Module::registerExtension(Extension& extension) {
  logger_->log_trace("Registering extension '%s' in module '%s'", extension.getName(), name_);
  std::lock_guard<std::mutex> guard(mtx_);
  extensions_.push_back(&extension);
}

bool Module::unregisterExtension(Extension& extension) {
  logger_->log_trace("Trying to unregister extension '%s' in module '%s'", extension.getName(), name_);
  std::lock_guard<std::mutex> guard(mtx_);
  auto it = std::find(extensions_.begin(), extensions_.end(), &extension);
  if (it == extensions_.end()) {
    logger_->log_error("Couldn't find extension '%s' in module '%s'", extension.getName(), name_);
    return false;
  }
  extensions_.erase(it);
  logger_->log_trace("Successfully unregistered extension '%s' in module '%s'", extension.getName(), name_);
  return true;
}

bool Module::initialize(const std::shared_ptr<Configure> &config) {
  logger_->log_trace("Initializing module '%s'", name_);
  std::lock_guard<std::mutex> guard(mtx_);
  for (auto* extension : extensions_) {
    logger_->log_trace("Initializing extension '%s'", extension->getName());
    if (!extension->initialize(config)) {
      logger_->log_error("Failed to initialize extension '%s' in module '%s'", extension->getName(), name_);
      return false;
    }
  }
  return true;
}

}  // namespace extension
}  // namespace core
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
