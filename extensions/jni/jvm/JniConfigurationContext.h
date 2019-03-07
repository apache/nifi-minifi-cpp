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

#ifndef EXTENSIONS_JNI_JVM_JNICONFIGURATIONCONTEXT_H_
#define EXTENSIONS_JNI_JVM_JNICONFIGURATIONCONTEXT_H_

#include "core/controller/ControllerService.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace jni {

class ConfigurationContext : public core::controller::ControllerService {
 public:

  // Constructor
  /*!
   * Create a new processor
   */
  explicit ConfigurationContext(std::string name, utils::Identifier uuid = utils::Identifier())
      : core::controller::ControllerService(name, uuid) {
  }

  explicit ConfigurationContext(const std::string &name, const std::string &id)
      : core::controller::ControllerService(name, id) {
  }

  virtual ~ConfigurationContext() {

  }
  virtual jobject getClassInstance() = 0;
};

struct JniConfigurationContext {

  std::shared_ptr<ConfigurationContext> service_reference_;
};

} /* namespace jni */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

#endif /* EXTENSIONS_JNI_JVM_JNICONFIGURATIONCONTEXT_H_ */
