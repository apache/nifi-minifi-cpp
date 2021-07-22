/**
 *
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

#include "ExecuteJavaControllerService.h"

#include <regex>
#include <memory>
#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <iterator>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "core/FlowFile.h"
#include "core/logging/Logger.h"
#include "core/ProcessContext.h"
#include "core/Relationship.h"
#include "ResourceClaim.h"
#include "utils/StringUtils.h"
#include "utils/ByteArrayCallback.h"
#include "jvm/JniMethod.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace jni {
namespace controllers {

core::Property ExecuteJavaControllerService::NiFiControllerService(
    core::PropertyBuilder::createProperty("NiFi Controller Service")->withDescription("Name of NiFi Controller Service to load and run")->isRequired(true)->withDefaultValue<std::string>("")->build());

const char *ExecuteJavaControllerService::ProcessorName = "ExecuteJavaControllerService";

void ExecuteJavaControllerService::initialize() {
  logger_->log_info("Initializing ExecuteJavaControllerService");
  // Set the supported properties
  std::string existingValue;
  getProperty(NiFiControllerService.getName(), existingValue);
  std::set<core::Property> properties;
  properties.insert(NiFiControllerService);
  setSupportedProperties(properties);
  setAcceptAllProperties();

  if (!existingValue.empty()) {
    setProperty(NiFiControllerService, existingValue);
  }
}

ExecuteJavaControllerService::~ExecuteJavaControllerService() = default;

void ExecuteJavaControllerService::onEnable() {
  std::string controller_service_name;

  auto serv_cs = JVMLoader::getInstance()->getBaseServicer();
  java_servicer_ = std::static_pointer_cast<controllers::JavaControllerService>(serv_cs);
  if (serv_cs == nullptr)
    throw std::runtime_error("Could not load controller service");

  if (!getProperty(NiFiControllerService.getName(), class_name_)) {
    throw std::runtime_error("NiFi Controller Service must be defined");
  }

  auto env = java_servicer_->attach();

  ClassRegistrar::getRegistrar().registerClasses(env, java_servicer_, "org/apache/nifi/processor/JniConfigurationContext", getJniConfigurationContext());
  ClassRegistrar::getRegistrar().registerClasses(env, java_servicer_, "org/apache/nifi/processor/JniInitializationContext", getJniInitializationContextSignatures());
  config_context_.service_reference_ = shared_from_this();

  contextInstance = java_servicer_->newInstance("org.apache.nifi.processor.JniConfigurationContext");

  java_servicer_->setReference<minifi::jni::JniConfigurationContext>(env, contextInstance, &config_context_);

  clazzInstance = java_servicer_->newInstance(class_name_);

  auto methods_with_signatures = java_servicer_->getAnnotations(class_name_, "OnEnabled");
  current_cs_class = java_servicer_->getObjectClass(class_name_, clazzInstance);
  try {
    for (const auto &mwithsig : methods_with_signatures) {
      current_cs_class.callVoidMethod(env, clazzInstance, mwithsig.first, mwithsig.second, contextInstance);
    }
  } catch (std::runtime_error &re) {
    // this can be ignored.
  }
}

REGISTER_RESOURCE(ExecuteJavaControllerService, "ExecuteJavaClass runs NiFi Controller services given a provided system path ");

} /* namespace controllers */
} /* namespace jni */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

