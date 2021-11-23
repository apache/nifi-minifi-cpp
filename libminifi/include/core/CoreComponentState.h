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

#ifndef LIBMINIFI_INCLUDE_CORE_CORECOMPONENTSTATE_H_
#define LIBMINIFI_INCLUDE_CORE_CORECOMPONENTSTATE_H_

#include "Core.h"

#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace core {

using CoreComponentState = std::unordered_map<std::string, std::string>;

class CoreComponentStateManager {
 public:
  virtual ~CoreComponentStateManager() = default;

  virtual bool set(const CoreComponentState& kvs) = 0;
  virtual bool get(CoreComponentState& kvs) = 0;

  std::optional<std::unordered_map<std::string, std::string>> get() {
    std::unordered_map<std::string, std::string> out;
    if (get(out)) {
      return out;
    } else {
      return std::nullopt;
    }
  }

  virtual bool clear() = 0;
  virtual bool persist() = 0;

  virtual bool isTransactionInProgress() const = 0;
  virtual bool beginTransaction() = 0;
  virtual bool commit() = 0;
  virtual bool rollback() = 0;
};

class CoreComponentStateManagerProvider {
 public:
  virtual ~CoreComponentStateManagerProvider() = default;

  virtual std::unique_ptr<CoreComponentStateManager> getCoreComponentStateManager(const utils::Identifier& uuid) = 0;

  virtual std::unique_ptr<CoreComponentStateManager> getCoreComponentStateManager(const CoreComponent& component) {
    return getCoreComponentStateManager(component.getUUID());
  }

  virtual std::map<utils::Identifier, CoreComponentState> getAllCoreComponentStates() = 0;
};

}  // namespace core
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org

#endif  // LIBMINIFI_INCLUDE_CORE_CORECOMPONENTSTATE_H_
