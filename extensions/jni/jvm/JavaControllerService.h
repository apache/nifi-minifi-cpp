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

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace jni {
namespace controllers {

/**
 * Purpose and Justification: Java Controller Service is intended to be used either within the flow or
 * based on a static load in JVM Creator. The static load simply loads via minifi properties.
 *
 */
class JavaControllerService : public core::controller::ControllerService, public std::enable_shared_from_this<JavaControllerService>, public JavaServicer {
 public:
  explicit JavaControllerService(const std::string &name, const utils::Identifier &uuid = {})
      : ControllerService(name, uuid),
        loader(nullptr),
        logger_(logging::LoggerFactory<JavaControllerService>::getLogger()) {
    initialized_ = false;
  }

  explicit JavaControllerService(const std::string &name, const std::shared_ptr<Configure> &configuration)
      : ControllerService(name),
        loader(nullptr),
        logger_(logging::LoggerFactory<JavaControllerService>::getLogger()) {
    initialized_ = false;
    setConfiguration(configuration);
    initialize();
  }

  static core::Property NarDirectory;
  static core::Property NarDeploymentDirectory;
  static core::Property NarDocumentDirectory;

  void initialize() override;

  void yield() override {
  }

  bool isRunning() override {
    return getState() == core::controller::ControllerServiceState::ENABLED;
  }

  bool isWorkAvailable() override {
    return false;
  }

  void onEnable() override;

  std::vector<std::string> getPaths() const {
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

  std::atomic<bool> initialized_;

  std::vector<std::string> classpaths_;

  std::unique_ptr<NarClassLoader> nar_loader_;

  JVMLoader *loader;

  std::shared_ptr<logging::Logger> logger_;
};

} /* namespace controllers */
} /* namespace jni */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
