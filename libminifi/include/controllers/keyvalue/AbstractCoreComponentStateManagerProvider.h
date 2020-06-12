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

#include "core/Core.h"
#include "core/CoreComponentState.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace controllers {

class AbstractCoreComponentStateManagerProvider : public std::enable_shared_from_this<AbstractCoreComponentStateManagerProvider>,
                                                   public core::CoreComponentStateManagerProvider {
 public:
  virtual ~AbstractCoreComponentStateManagerProvider();

  std::shared_ptr<core::CoreComponentStateManager> getCoreComponentStateManager(const std::string& uuid) override;

  std::unordered_map<std::string, std::unordered_map<std::string, std::string>> getAllCoreComponentStates() override;

  class AbstractCoreComponentStateManager : public core::CoreComponentStateManager{
   private:
    std::shared_ptr<AbstractCoreComponentStateManagerProvider> provider_;
    std::string id_;
    bool state_valid_;
    std::unordered_map<std::string, std::string> state_;

   public:
    AbstractCoreComponentStateManager(std::shared_ptr<AbstractCoreComponentStateManagerProvider> provider, const std::string& id);

    bool set(const std::unordered_map<std::string, std::string>& kvs) override;

    bool get(std::unordered_map<std::string, std::string>& kvs) override;

    bool clear() override;

    bool persist() override;
  };

 protected:
  virtual bool setImpl(const std::string& key, const std::string& value) = 0;
  virtual bool getImpl(const std::string& key, std::string& value) = 0;
  virtual bool getImpl(std::unordered_map<std::string, std::string>& kvs) = 0;
  virtual bool removeImpl(const std::string& key) = 0;
  virtual bool persistImpl() = 0;

  virtual std::string serialize(const std::unordered_map<std::string, std::string>& kvs);
  bool deserialize(const std::string& serialized, std::unordered_map<std::string, std::string>& kvs);
};

}  // namespace controllers
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org

#endif  // LIBMINIFI_INCLUDE_CONTROLLERS_KEYVALUE_ABSTRACTCORECOMPONENTSTATEMANAGERPROVIDER_H_
