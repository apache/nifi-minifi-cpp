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

#include "JavaControllerService.h"

#include <string>
#include <memory>
#include <algorithm>
#include <iterator>
#include <set>
#include "core/Property.h"
#include "core/Resource.h"
#include "io/validation.h"
#include "utils/StringUtils.h"
#include "utils/file/FileUtils.h"
#include "properties/Configure.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace jni {
namespace controllers {

#ifndef S_ISDIR
#define S_ISDIR(mode)  (((mode) & S_IFMT) == S_IFDIR)
#endif
#ifndef R_OK
#define R_OK    4       /* Test for read permission.  */
#define W_OK    2       /* Test for write permission.  */
#define F_OK    0       /* Test for existence.  */
#endif
static core::Property NarDirectory;
static core::Property NarDeploymentDirectory;
static core::Property NarDocumentDirectory;

core::Property JavaControllerService::NarDirectory(
    core::PropertyBuilder::createProperty("Nar Directory")->withDescription("Directory containing the nars to deploy")->isRequired(true)->supportsExpressionLanguage(false)->build());

core::Property JavaControllerService::NarDeploymentDirectory(
    core::PropertyBuilder::createProperty("Nar Deployment Directory")->withDescription("Directory in which nars will be deployed")->isRequired(true)->supportsExpressionLanguage(false)->build());

core::Property JavaControllerService::NarDocumentDirectory(
    core::PropertyBuilder::createProperty("Nar Document Directory")->withDescription("Directory in which documents will be deployed")->isRequired(true)->supportsExpressionLanguage(false)->build());

void JavaControllerService::initialize() {
  if (initialized_)
    return;

  std::lock_guard<std::mutex> lock(initialization_mutex_);

  ControllerService::initialize();

  std::set<core::Property> supportedProperties;
  supportedProperties.insert(NarDirectory);
  supportedProperties.insert(NarDeploymentDirectory);
  supportedProperties.insert(NarDocumentDirectory);

  setSupportedProperties(supportedProperties);

  initialized_ = true;
}

void JavaControllerService::onEnable() {
  std::vector<std::string> pathOrFiles;

  core::Property prop = NarDirectory;

  std::string nardir, narscratch, nardocs;
  if (getProperty(NarDirectory.getName(), prop)) {
    nardir = prop.getValue().to_string();
  }

  prop = NarDeploymentDirectory;

  if (getProperty(NarDeploymentDirectory.getName(), prop)) {
    narscratch = prop.getValue().to_string();
  }

  prop = NarDocumentDirectory;

  if (getProperty(NarDocumentDirectory.getName(), prop)) {
    nardocs = prop.getValue().to_string();
  }

  for (const auto &path : pathOrFiles) {
    utils::file::FileUtils::addFilesMatchingExtension(logger_, path, ".jar", classpaths_);
  }

  loader = JVMLoader::getInstance();

  narClassLoaderClazz = loadClass("org/apache/nifi/processor/JniClassLoader");

  nar_loader_ = std::unique_ptr<NarClassLoader>(new NarClassLoader(shared_from_this(), narClassLoaderClazz, nardir, narscratch, nardocs));
}

REGISTER_RESOURCE(JavaControllerService, "Allows specification of nars to be used within referenced processors. ");

} /* namespace controllers */
} /* namespace jni */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
