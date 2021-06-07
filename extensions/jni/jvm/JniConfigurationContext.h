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

#include <string>
#include <memory>

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
  explicit ConfigurationContext(const std::string& name, const utils::Identifier& uuid = {})
      : core::controller::ControllerService(name, uuid) {
  }

  virtual ~ConfigurationContext() = default;
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
