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
#pragma once

#include <iostream>
#include <memory>
#include <vector>
#include <string>
#include <utility>
#include <map>

#include "../jvm/JVMLoader.h"
#include "NarClassLoader.h"
#include "utils/StringUtils.h"
#include "JavaServicer.h"
#include "io/validation.h"
#include "core/controller/ControllerService.h"
#include "core/logging/LoggerConfiguration.h"
#include "core/PropertyDefinition.h"
#include "core/PropertyDefinitionBuilder.h"

namespace org::apache::nifi::minifi::jni::controllers {

/**
 * Purpose and Justification: Java Controller Service is intended to be used either within the flow or
 * based on a static load in JVM Creator. The static load simply loads via minifi properties.
 */
class JavaControllerService : public core::controller::ControllerService, public std::enable_shared_from_this<JavaControllerService>, public JavaServicer {
 public:
  explicit JavaControllerService(std::string name, const utils::Identifier &uuid = {})
      : ControllerService(std::move(name), uuid) {
  }

  explicit JavaControllerService(const std::string &name, const std::shared_ptr<Configure> &configuration)
      : ControllerService(name) {
    setConfiguration(configuration);
    initialize();
  }

  EXTENSIONAPI static constexpr const char* Description = "Allows specification of nars to be used within referenced processors.";

  EXTENSIONAPI static constexpr auto NarDirectory = core::PropertyDefinitionBuilder<>::createProperty("Nar Directory")
      .withDescription("Directory containing the nars to deploy")
      .isRequired(true)
      .supportsExpressionLanguage(false)
      .build();
  EXTENSIONAPI static constexpr auto NarDeploymentDirectory = core::PropertyDefinitionBuilder<>::createProperty("Nar Deployment Directory")
      .withDescription("Directory in which nars will be deployed")
      .isRequired(true)
      .supportsExpressionLanguage(false)
      .build();
  EXTENSIONAPI static constexpr auto NarDocumentDirectory = core::PropertyDefinitionBuilder<>::createProperty("Nar Document Directory")
      .withDescription("Directory in which documents will be deployed")
      .isRequired(true)
      .supportsExpressionLanguage(false)
      .build();
  EXTENSIONAPI static constexpr auto Properties = std::array<core::PropertyReference, 3>{
      NarDirectory,
      NarDeploymentDirectory,
      NarDocumentDirectory
  };


  EXTENSIONAPI static constexpr bool SupportsDynamicProperties = false;
  ADD_COMMON_VIRTUAL_FUNCTIONS_FOR_CONTROLLER_SERVICES

  void initialize() override;

  void yield() override {
  }

  bool isRunning() const override {
    return getState() == core::controller::ControllerServiceState::ENABLED;
  }

  bool isWorkAvailable() override {
    return false;
  }

  void onEnable() override;

  std::vector<std::filesystem::path> getPaths() const {
    return classpaths_;
  }

  JavaClass getObjectClass(const std::string &name, jobject jobj) {
    return loader->getObjectClass(name, jobj);
  }

  JavaClass loadClass(const std::string &class_name_) override {
    std::string modifiedName = class_name_;
    modifiedName = utils::StringUtils::replaceAll(modifiedName, ".", "/");
    return loader->load_class(modifiedName);
  }

  JNIEnv *attach() override {
    return loader->attach();
  }

  void detach() override {
      loader->detach();
    }

  jobject getClassLoader() override {
    return loader->getClassLoader();
  }

  template<typename T>
  void setReference(JNIEnv *env, jobject obj, T *t) {
    loader->setReference(obj, env, t);
  }

  template<typename T>
  void putNativeFunctionMapping(JNIEnv *env, JavaClass &clazz, const std::string &fieldStr = "nativePtr", const std::string &arg = "J") {
    JVMLoader::putClassMapping<T>(env, clazz, fieldStr, arg);
  }

  /**
   * Retrieves a matching annotation
   * @param requested class name,
   * @param method_name method name to obtain
   */
  std::pair<std::string, std::string> getAnnotation(const std::string &requested_name, const std::string &method_name) {
    return nar_loader_->getAnnotation(requested_name, method_name);
  }

  /**
     * Retrieves a matching annotation
     * @param requested class name,
     * @param method_name method name to obtain
     */
    std::map<std::string, std::string> getAnnotations(const std::string &requested_name, const std::string &annotation_name) {
      return nar_loader_->getAnnotations(requested_name, annotation_name);
    }
  /**
   * creates a new instance
   * @param requested_name reqeusted class name
   */
  jobject newInstance(const std::string &requested_name) {
    return nar_loader_->newInstance(requested_name);
  }

 private:
  JavaClass narClassLoaderClazz;

  std::mutex initialization_mutex_;

  bool initialized_ = false;

  std::vector<std::filesystem::path> classpaths_;

  std::unique_ptr<NarClassLoader> nar_loader_;

  JVMLoader *loader = nullptr;

  std::shared_ptr<core::logging::Logger> logger_ = core::logging::LoggerFactory<JavaControllerService>::getLogger(uuid_);
};

}  // namespace org::apache::nifi::minifi::jni::controllers
