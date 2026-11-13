// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#pragma once

#include <utility>
#include <vector>
#include <memory>
#include <string>
#include <optional>
#include <unordered_map>

#include "core/Core.h"
#include "minifi-cpp/core/state/nodes/MetricsBase.h"
#include "core/state/PublishedMetricProvider.h"

namespace org::apache::nifi::minifi::state::response {

class ResponseNode;
using SharedResponseNode = gsl::not_null<std::shared_ptr<ResponseNode>>;

/**
 * Purpose: Defines a metric. Serialization is intended to be thread safe.
 */
class ResponseNodeImpl : public core::CoreComponentImpl, public PublishedMetricProviderImpl, public virtual ResponseNode {
 public:
  ResponseNodeImpl()
      : core::CoreComponentImpl("metric"),
        is_array_(false) {
  }

  explicit ResponseNodeImpl(const std::string_view name)
      : core::CoreComponentImpl(name),
        is_array_(false) {
  }

  ResponseNodeImpl(const std::string_view name, const utils::Identifier& uuid)
      : core::CoreComponentImpl(name, uuid),
        is_array_(false) {
  }

  ~ResponseNodeImpl() override = default;

  static std::vector<SerializedResponseNode> serializeAndMergeResponseNodes(const std::vector<SharedResponseNode>& nodes);

  bool isArray() const override {
    return is_array_;
  }

  bool isEmpty() override {
    return false;
  }

 protected:
  bool is_array_;

  void setArray(bool array) {
    is_array_ = array;
  }
};

}  // namespace org::apache::nifi::minifi::state::response
