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

#ifndef LIBMINIFI_INCLUDE_CORE_CoreComponentState_H_
#define LIBMINIFI_INCLUDE_CORE_CoreComponentState_H_

#include "Core.h"

#include <cstdint>
#include <memory>
#include <unordered_map>
#include <string>

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace core {

class CoreComponentStateManager {
 public:
  virtual ~CoreComponentStateManager() {
  }

  virtual bool set(const std::unordered_map<std::string, std::string>& kvs) = 0;

  virtual bool get(std::unordered_map<std::string, std::string>& kvs) = 0;

  virtual bool clear() = 0;

  virtual bool persist() = 0;
};

class CoreComponentStateManagerProvider {
 public:
  virtual ~CoreComponentStateManagerProvider() = default;

  virtual std::shared_ptr<CoreComponentStateManager> getCoreComponentStateManager(const std::string& uuid) = 0;

  virtual std::shared_ptr<CoreComponentStateManager> getCoreComponentStateManager(const CoreComponent& component) {
    return getCoreComponentStateManager(component.getUUIDStr());
  }

  virtual std::unordered_map<std::string, std::unordered_map<std::string, std::string>> getAllCoreComponentStates() = 0;
};

} /* namespace core */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

#endif /* LIBMINIFI_INCLUDE_CORE_CoreComponentState_H_ */
