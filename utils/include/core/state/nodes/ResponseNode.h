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

#include <utility>
#include <vector>
#include <memory>
#include <string>
#include <optional>
#include <unordered_map>

#include "core/Core.h"
#include "core/Connectable.h"
#include "minifi-cpp/core/state/nodes/MetricsBase.h"
#include "core/state/PublishedMetricProvider.h"

namespace org::apache::nifi::minifi::state::response {

class ResponseNode;
using SharedResponseNode = gsl::not_null<std::shared_ptr<ResponseNode>>;

/**
 * Purpose: Defines a metric. Serialization is intended to be thread safe.
 */
class ResponseNodeImpl : public core::ConnectableImpl, public PublishedMetricProviderImpl, public virtual ResponseNode {
 public:
  ResponseNodeImpl()
      : core::ConnectableImpl("metric"),
        is_array_(false) {
  }

  explicit ResponseNodeImpl(const std::string_view name)
      : core::ConnectableImpl(name),
        is_array_(false) {
  }

  ResponseNodeImpl(const std::string_view name, const utils::Identifier& uuid)
      : core::ConnectableImpl(name, uuid),
        is_array_(false) {
  }

  ~ResponseNodeImpl() override = default;

  static std::vector<SerializedResponseNode> serializeAndMergeResponseNodes(const std::vector<SharedResponseNode>& nodes);

  void yield() override {
  }

  bool isRunning() const override {
    return true;
  }

  bool isWorkAvailable() override {
    return true;
  }

  bool isArray() const override {
    return is_array_;
  }

  virtual bool isEmpty() override {
    return false;
  }

 protected:
  bool is_array_;

  void setArray(bool array) {
    is_array_ = array;
  }
};

}  // namespace org::apache::nifi::minifi::state::response
