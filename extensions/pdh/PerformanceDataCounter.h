// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#pragma once

#include <string>

#include "rapidjson/document.h"

namespace org::apache::nifi::minifi::processors {

class PerformanceDataCounter {
 public:
  PerformanceDataCounter() = default;
  virtual ~PerformanceDataCounter() = default;

  virtual bool collectData() = 0;
  virtual void addToJson(rapidjson::Value& body, rapidjson::Document::AllocatorType& alloc) const = 0;

 protected:
  static rapidjson::Value& acquireNode(const std::string& node_name, rapidjson::Value& parent_node, rapidjson::Document::AllocatorType& alloc) {
    if (!parent_node.HasMember(node_name.c_str())) {
      rapidjson::Value value(rapidjson::kObjectType);
      rapidjson::Value key(node_name.c_str(), alloc);
      parent_node.AddMember(key, value, alloc);
    }
    return parent_node[node_name.c_str()];
  }
};

}  // namespace org::apache::nifi::minifi::processors
