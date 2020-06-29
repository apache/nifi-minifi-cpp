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
#ifndef LIBMINIFI_INCLUDE_CORE_STATE_UPDATEPOLICY_H_
#define LIBMINIFI_INCLUDE_CORE_STATE_UPDATEPOLICY_H_

#include <memory>
#include <string>
#include <utility>
#include <unordered_map>

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace state {

enum UPDATE_POLICY {
  PERM_ALLOWED,
  PERM_DISALLOWED,
  IMPLICIT_DISALLOWED,
  TEMP_ALLOWED
};

class UpdatePolicyBuilder;
/**
 * Purpose: Defines an immutable policy map.
 * Defines what can and cannot be updated during runtime.
 */
class UpdatePolicy {
 public:
  explicit UpdatePolicy(bool enable_all)
      : enable_all_(enable_all) {
  }

  bool canUpdate(const std::string &property) const {
    auto p = properties_.find(property);
    if (p != std::end(properties_)) {
      switch (p->second) {
        case PERM_ALLOWED:
        case TEMP_ALLOWED:
          return true;
        default:
          return false;
      }
    }

    return enable_all_;
  }

  UpdatePolicy &operator=(UpdatePolicy &&other) = default;

 protected:
  UpdatePolicy(const UpdatePolicy &other) = default;

  UpdatePolicy(UpdatePolicy &&other) = default;

  UpdatePolicy() = delete;

  friend class UpdatePolicyBuilder;

  void setEnableAll(bool enable_all) {
    enable_all_ = enable_all;
  }

  void setProperty(const std::string &property, UPDATE_POLICY policy) {
    properties_.emplace(property, policy);
  }

  bool enable_all_;

  std::unordered_map<std::string, UPDATE_POLICY> properties_;
};

/**
 * Purpose: Defines a builder for UpdatePolicy definitions. Since policies are made
 * to be immutable defining an object as const isn't sufficient as const can be casted
 * away. The builder helps add a layer to avoiding this by design.
 */
class UpdatePolicyBuilder {
 public:
  static std::unique_ptr<UpdatePolicyBuilder> newBuilder(bool enable_all = false) {
    std::unique_ptr<UpdatePolicyBuilder> policy = std::unique_ptr<UpdatePolicyBuilder>( new UpdatePolicyBuilder(enable_all));
    return policy;
  }

  void allowPropertyUpdate(const std::string &property) {
    current_policy_->setProperty(property, PERM_ALLOWED);
  }

  void disallowPropertyUpdate(const std::string &property) {
    current_policy_->setProperty(property, PERM_DISALLOWED);
  }

  std::unique_ptr<UpdatePolicy> build() {
    std::unique_ptr<UpdatePolicy> new_policy = std::unique_ptr<UpdatePolicy>(new UpdatePolicy(*(current_policy_.get())));
    return new_policy;
  }


 protected:
  UpdatePolicyBuilder(const UpdatePolicyBuilder &other) = default;

  explicit UpdatePolicyBuilder(bool enable_all) {
    current_policy_ = std::make_shared<UpdatePolicy>(enable_all);
  }

  std::shared_ptr<UpdatePolicy> current_policy_;
};

}  // namespace state
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org

#endif  // LIBMINIFI_INCLUDE_CORE_STATE_UPDATEPOLICY_H_
