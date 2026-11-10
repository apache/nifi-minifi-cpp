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
#include "core/state/nodes/ResponseNode.h"

namespace org::apache::nifi::minifi::state::response {

class ResponseNodeSource {
 public:
  virtual ~ResponseNodeSource() = default;
  virtual SharedResponseNode getResponseNode() = 0;
};

class DeviceInformation : public ResponseNodeImpl {
 public:
  DeviceInformation(std::string_view name, const utils::Identifier& uuid)
      : ResponseNodeImpl(name, uuid) {
  }

  explicit DeviceInformation(std::string_view name)
      : ResponseNodeImpl(name) {
  }
};

class ObjectNode : public ResponseNodeImpl {
 public:
  explicit ObjectNode(const std::string_view name, const utils::Identifier& uuid = {})
      : ResponseNodeImpl(name, uuid) {
  }

  void add_node(const SharedResponseNode &node) {
    nodes_[node->getName()].push_back(node);
  }

  std::string getName() const override {
    return CoreComponentImpl::getName();
  }

  std::vector<SerializedResponseNode> serialize() override;

  bool isEmpty() override {
    return nodes_.empty();
  }

 protected:
  std::unordered_map<std::string, std::vector<SharedResponseNode>> nodes_;
};

}  // namespace org::apache::nifi::minifi::state::response
