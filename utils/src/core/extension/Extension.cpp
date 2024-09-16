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

#include "core/extension/Extension.h"
#include "minifi-cpp/core/extension/ExtensionManager.h"
#include "core/logging/LoggerFactory.h"

namespace org::apache::nifi::minifi::core::extension {

static std::shared_ptr<logging::Logger> init_logger = logging::LoggerFactory<ExtensionInitializer>::getLogger();

ExtensionImpl::ExtensionImpl(std::string name, ExtensionInitImpl init_impl, ExtensionDeinitImpl deinit_impl, ExtensionInit init)
    : name_(std::move(name)), init_impl_(init_impl), deinit_impl_(deinit_impl), init_(init) {
  ExtensionManager::get().registerExtension(*this);
}

ExtensionImpl::~ExtensionImpl() {
  ExtensionManager::get().unregisterExtension(*this);
}

ExtensionInitializer::ExtensionInitializer(ExtensionImpl& extension, const ExtensionConfig& config): extension_(extension) {
  init_logger->log_trace("Initializing extension: {}", extension_.getName());
  if (!extension_.init_impl_(config)) {
    throw std::runtime_error("Failed to initialize extension");
  }
}
ExtensionInitializer::~ExtensionInitializer() {
  init_logger->log_trace("Deinitializing extension: {}", extension_.getName());
  extension_.deinit_impl_();
}

}  // namespace org::apache::nifi::minifi::core::extension
