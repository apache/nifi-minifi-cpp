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
#include "core/ClassLoader.h"
#include "core/logging/LoggerConfiguration.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace core {

ClassLoader::ClassLoader(const std::string& name)
  : logger_(logging::LoggerFactory<ClassLoader>::getLogger()), name_(name) {}

ClassLoader &ClassLoader::getDefaultClassLoader() {
  static ClassLoader ret;
  // populate ret
  return ret;
}

ClassLoader& ClassLoader::getClassLoader(const std::string& child_name) {
  std::lock_guard<std::mutex> lock(internal_mutex_);
  auto it = class_loaders_.find(child_name);
  if (it != class_loaders_.end()) {
    return it->second;
  }
  std::string full_name = [&] {
    if (name_ == "/") {
      return "/" + child_name;
    }
    return name_ + "/" + child_name;
  }();
  ClassLoader& child = class_loaders_[child_name];
  child.name_ = std::move(full_name);
  return child;
}

} /* namespace core */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
