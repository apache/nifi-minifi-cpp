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
#include "minifi-cpp/core/state/FlowIdentifier.h"


namespace org::apache::nifi::minifi::state {

/**
 * Purpose: Represents a flow identifier for a given flow update or instance.
 *
 * Design: Immutable collection of strings for the component parts.
 */
class FlowIdentifierImpl : public virtual FlowIdentifier {
 public:
  FlowIdentifierImpl() = delete;

  explicit FlowIdentifierImpl(const std::string &url, const std::string &bucket_id, const std::string &flow_id) {
    registry_url_ = url;
    bucket_id_ = bucket_id;
    flow_id_ = flow_id;
  }

  /**
   * In most cases the lock guard isn't necessary for these getters; however,
   * we don't want to cause issues if the FlowVersion object is ever used in a way
   * that breaks the current paradigm.
   */
  std::string getRegistryUrl() const override {
    return registry_url_;
  }

  std::string getBucketId() const override {
    return bucket_id_;
  }

  std::string getFlowId() const override {
    return flow_id_;
  }

 protected:
  FlowIdentifierImpl(const FlowIdentifierImpl &other) = default;
  FlowIdentifierImpl &operator=(const FlowIdentifierImpl &other) = default;

 private:
  std::string registry_url_;
  std::string bucket_id_;
  std::string flow_id_;
  friend class FlowVersion;
};


}  // namespace org::apache::nifi::minifi::state
