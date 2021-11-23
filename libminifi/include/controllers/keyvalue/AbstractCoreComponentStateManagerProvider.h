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
#ifndef LIBMINIFI_INCLUDE_CONTROLLERS_KEYVALUE_ABSTRACTCORECOMPONENTSTATEMANAGERPROVIDER_H_
#define LIBMINIFI_INCLUDE_CONTROLLERS_KEYVALUE_ABSTRACTCORECOMPONENTSTATEMANAGERPROVIDER_H_

#include <unordered_map>
#include <string>
#include <memory>
#include <map>

#include "core/Core.h"
#include "core/CoreComponentState.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace controllers {

class AbstractCoreComponentStateManagerProvider : public core::CoreComponentStateManagerProvider {
 public:
  std::unique_ptr<core::CoreComponentStateManager> getCoreComponentStateManager(const utils::Identifier& uuid) override;

  std::map<utils::Identifier, std::unordered_map<std::string, std::string>> getAllCoreComponentStates() override;

  class AbstractCoreComponentStateManager : public core::CoreComponentStateManager {
   public:
    AbstractCoreComponentStateManager(AbstractCoreComponentStateManagerProvider* provider, const utils::Identifier& id);

    bool set(const core::CoreComponentState& kvs) override;
    bool get(core::CoreComponentState& kvs) override;
    bool clear() override;
    bool persist() override;

    bool isTransactionInProgress() const override;
    bool beginTransaction() override;
    bool commit() override;
    bool rollback() override;

   private:
    enum class ChangeType {
      NONE,
      SET,
      CLEAR
    };

    AbstractCoreComponentStateManagerProvider* provider_;
    utils::Identifier id_;
    bool state_valid_;
    core::CoreComponentState state_;
    bool transaction_in_progress_;
    ChangeType change_type_;
    core::CoreComponentState state_to_set_;
  };

 protected:
  virtual bool setImpl(const utils::Identifier& key, const std::string& serialized_state) = 0;
  virtual bool getImpl(const utils::Identifier& key, std::string& serialized_state) = 0;
  virtual bool getImpl(std::map<utils::Identifier, std::string>& kvs) = 0;
  virtual bool removeImpl(const utils::Identifier& key) = 0;
  virtual bool persistImpl() = 0;

 private:
  std::mutex mutex_;
};

}  // namespace controllers
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org

#endif  // LIBMINIFI_INCLUDE_CONTROLLERS_KEYVALUE_ABSTRACTCORECOMPONENTSTATEMANAGERPROVIDER_H_
