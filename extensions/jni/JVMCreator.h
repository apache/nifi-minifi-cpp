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

#include <vector>
#include <string>
#include <memory>
#include "jvm/JVMLoader.h"
#include "jvm/JavaControllerService.h"
#include "utils/file/FileUtils.h"
#include "core/Core.h"
#include "core/logging/LoggerConfiguration.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace jni {

/**
 * Can be used to load the JVM from NiFi properties.
 */
class JVMCreator : public minifi::core::CoreComponent {
 public:
  explicit JVMCreator(const std::string &name, const utils::Identifier &uuid = {})
      : minifi::core::CoreComponent(name, uuid),
        loader_(nullptr),
        logger_(logging::LoggerFactory<JVMCreator>::getLogger()) {
  }

  virtual ~JVMCreator();

  void configure(const std::vector<std::string> &jarFileListings) {
    std::vector<std::string> pathOrFiles;
    for (const auto &path : jarFileListings) {
      const auto vec = utils::StringUtils::split(path, ",");
      pathOrFiles.insert(pathOrFiles.end(), vec.begin(), vec.end());
    }

    for (const auto &path : pathOrFiles) {
      logger_->log_debug("Adding path %s", path);
      minifi::utils::file::FileUtils::addFilesMatchingExtension(logger_, path, ".jar", classpaths_);
    }
  }

  void configure(const std::shared_ptr<Configure> &configuration) override {
    std::string pathListings, jvmOptionsStr;

    // assuming we have the options set and can access the JVMCreator

    if (configuration->get("nifi.framework.dir", pathListings)) {
      std::vector<std::string> paths;
      paths.emplace_back(pathListings);
      configure(paths);

      if (configuration->get("nifi.jvm.options", jvmOptionsStr)) {
        jvm_options_ = utils::StringUtils::split(jvmOptionsStr, ",");
      }

      initializeJVM();
    }
    std::string nar_dir, nar_dep, nar_docs;
    if (loader_ && configuration->get("nifi.nar.directory", nar_dir) && configuration->get("nifi.nar.deploy.directory", nar_dep)) {
      std::shared_ptr<jni::controllers::JavaControllerService> servicer = std::make_shared<jni::controllers::JavaControllerService>("BaseService");
      servicer->initialize();
      servicer->setProperty(jni::controllers::JavaControllerService::NarDirectory, nar_dir);
      servicer->setProperty(jni::controllers::JavaControllerService::NarDeploymentDirectory, nar_dep);
      servicer->setProperty(jni::controllers::JavaControllerService::NarDocumentDirectory, nar_docs);
      servicer->onEnable();
      loader_->setBaseServicer(servicer);
    }
  }

  void initializeJVM() {
    loader_ = minifi::jni::JVMLoader::getInstance(classpaths_, jvm_options_);
  }

 private:
  minifi::jni::JVMLoader *loader_;

  std::vector<std::string> jvm_options_;

  std::vector<std::string> classpaths_;

  std::shared_ptr<logging::Logger> logger_;
};

} /* namespace jni */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
