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
#include "core/Resource.h"
#include "io/validation.h"
#include "utils/StringUtils.h"
#include "utils/file/FileUtils.h"
#include "properties/Configure.h"

namespace org::apache::nifi::minifi::jni::controllers {

#ifndef S_ISDIR
#define S_ISDIR(mode)  (((mode) & S_IFMT) == S_IFDIR)
#endif
#ifndef R_OK
#define R_OK    4       /* Test for read permission.  */
#define W_OK    2       /* Test for write permission.  */
#define F_OK    0       /* Test for existence.  */
#endif

void JavaControllerService::initialize() {
  std::lock_guard<std::mutex> lock(initialization_mutex_);
  if (initialized_) {
    return;
  }

  ControllerService::initialize();

  setSupportedProperties(Properties);

  initialized_ = true;
}

void JavaControllerService::onEnable() {
  std::vector<std::string> pathOrFiles;

  std::string nardir;
  std::string narscratch;
  std::string nardocs;
  getProperty(NarDirectory, nardir);
  getProperty(NarDeploymentDirectory, narscratch);
  getProperty(NarDocumentDirectory, nardocs);

  for (const auto &path : pathOrFiles) {
    utils::file::FileUtils::addFilesMatchingExtension(logger_, path, ".jar", classpaths_);
  }

  loader = JVMLoader::getInstance();

  narClassLoaderClazz = loadClass("org/apache/nifi/processor/JniClassLoader");

  nar_loader_ = std::make_unique<NarClassLoader>(shared_from_this(), narClassLoaderClazz, nardir, narscratch, nardocs);
}

REGISTER_RESOURCE(JavaControllerService, ControllerService);

}  // namespace org::apache::nifi::minifi::jni::controllers
