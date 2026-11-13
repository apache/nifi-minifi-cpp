// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#pragma once

#include <memory>
#include <string>
#include <utility>
#include <unordered_map>

namespace org::apache::nifi::minifi::state {

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
    std::unique_ptr<UpdatePolicyBuilder> policy = std::unique_ptr<UpdatePolicyBuilder>( new UpdatePolicyBuilder(enable_all));  // NOLINT(clang-analyzer-cplusplus.NewDeleteLeaks)
    return policy;
  }

  void allowPropertyUpdate(const std::string &property) {
    current_policy_->setProperty(property, PERM_ALLOWED);
  }

  void disallowPropertyUpdate(const std::string &property) {
    current_policy_->setProperty(property, PERM_DISALLOWED);
  }

  std::unique_ptr<UpdatePolicy> build() {
    std::unique_ptr<UpdatePolicy> new_policy = std::unique_ptr<UpdatePolicy>(new UpdatePolicy(*(current_policy_.get())));  // NOLINT(clang-analyzer-cplusplus.NewDeleteLeaks)
    return new_policy;
  }


 protected:
  UpdatePolicyBuilder(const UpdatePolicyBuilder &other) = default;

  explicit UpdatePolicyBuilder(bool enable_all) {
    current_policy_ = std::make_shared<UpdatePolicy>(enable_all);
  }

  std::shared_ptr<UpdatePolicy> current_policy_;
};

}  // namespace org::apache::nifi::minifi::state
