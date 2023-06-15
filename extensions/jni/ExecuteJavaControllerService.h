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

#include <memory>
#include <regex>
#include <string>
#include <utility>

#include "FlowFileRecord.h"
#include "core/controller/ControllerService.h"
#include "core/ProcessSession.h"
#include "core/Core.h"
#include "core/PropertyDefinition.h"
#include "core/PropertyDefinitionBuilder.h"
#include "concurrentqueue.h"
#include "core/logging/LoggerConfiguration.h"
#include "jvm/JavaControllerService.h"
#include "jvm/JniConfigurationContext.h"
#include "jvm/JniInitializationContext.h"
#include "utils/Id.h"
#include "jvm/NarClassLoader.h"
#include "ClassRegistrar.h"

namespace org::apache::nifi::minifi::jni::controllers {

/**
 * Purpose: Enables isolated java loading through the use of controller services
 *
 * While we do allow the nifi properties to define the classes, we also allow run
 * time  loading of java classes
 *
 * In the case where we load via properties, we effectively instantiate this
 * controller service within the execute java process.
 *
 */
class ExecuteJavaControllerService : public ConfigurationContext, public std::enable_shared_from_this<ConfigurationContext> {
 public:
  explicit ExecuteJavaControllerService(std::string name, const utils::Identifier& uuid = {})
      : ConfigurationContext(std::move(name), uuid) {
  }
  ~ExecuteJavaControllerService() override;

  EXTENSIONAPI static constexpr const char* Description = "ExecuteJavaClass runs NiFi Controller services given a provided system path";

  EXTENSIONAPI static constexpr auto NiFiControllerService = core::PropertyDefinitionBuilder<>::createProperty("NiFi Controller Service")
      .withDescription("Name of NiFi Controller Service to load and run")
      .isRequired(true)
      .build();
  EXTENSIONAPI static constexpr auto Properties = std::array<core::PropertyReference, 1>{NiFiControllerService};

  EXTENSIONAPI static constexpr bool SupportsDynamicProperties = true;
  ADD_COMMON_VIRTUAL_FUNCTIONS_FOR_CONTROLLER_SERVICES

  void onEnable() override;
  void initialize() override;

  void yield() override {
  }

  bool isRunning() const override {
    return getState() == core::controller::ControllerServiceState::ENABLED;
  }

  bool isWorkAvailable() override {
    return false;
  }

  void notifyStop() override {
    auto env = java_servicer_->attach();
    auto onEnabledName = java_servicer_->getAnnotation(class_name_, "OnDisabled");
    current_cs_class = java_servicer_->getObjectClass(class_name_, clazzInstance);
    // attempt to schedule here

    try {
      if (!onEnabledName.first.empty())
        current_cs_class.callVoidMethod(env, clazzInstance, onEnabledName.first, onEnabledName.second);
    } catch (std::runtime_error &re) {
      // this is avoidable.
    }

    if (clazzInstance)
      env->DeleteGlobalRef(clazzInstance);
    if (contextInstance)
      env->DeleteGlobalRef(contextInstance);
  }

  jobject getClassInstance() override {
    return clazzInstance;
  }

  static JavaSignatures &getJniConfigurationContext() {
    static JavaSignatures methodSignatures;
    if (methodSignatures.empty()) {
      methodSignatures.addSignature({ "getName", "()Ljava/lang/String;", reinterpret_cast<void*>(&Java_org_apache_nifi_processor_JniConfigurationContext_getName) });
      methodSignatures.addSignature({ "getComponent", "()Lorg/apache/nifi/components/AbstractConfigurableComponent;",
          reinterpret_cast<void*>(&Java_org_apache_nifi_processor_JniConfigurationContext_getComponent) });
      methodSignatures.addSignature({ "getPropertyNames", "()Ljava/util/List;", reinterpret_cast<void*>(&Java_org_apache_nifi_processor_JniConfigurationContext_getPropertyNames) });
      methodSignatures.addSignature(
          { "getPropertyValue", "(Ljava/lang/String;)Ljava/lang/String;", reinterpret_cast<void*>(&Java_org_apache_nifi_processor_JniConfigurationContext_getPropertyValue) });
    }
    return methodSignatures;
  }

  static JavaSignatures &getJniInitializationContextSignatures() {
    static JavaSignatures methodSignatures;
    if (methodSignatures.empty()) {
      methodSignatures.addSignature({ "getControllerServiceLookup", "()Lorg/apache/nifi/controller/ControllerServiceLookup;",
          reinterpret_cast<void*>(&Java_org_apache_nifi_processor_JniInitializationContext_getControllerServiceLookup) });
      methodSignatures.addSignature({ "getIdentifier", "()Ljava/lang/String;", reinterpret_cast<void*>(&Java_org_apache_nifi_processor_JniInitializationContext_getIdentifier) });
    }
    return methodSignatures;
  }

 private:
  minifi::jni::JniConfigurationContext config_context_;

  jobject contextInstance = nullptr;

  JavaClass current_cs_class;

  jobject clazzInstance = nullptr;

  std::shared_ptr<controllers::JavaControllerService> java_servicer_;

  std::string class_name_;

  std::shared_ptr<core::logging::Logger> logger_ = core::logging::LoggerFactory<ExecuteJavaControllerService>::getLogger(uuid_);
};

}  // namespace org::apache::nifi::minifi::jni::controllers
