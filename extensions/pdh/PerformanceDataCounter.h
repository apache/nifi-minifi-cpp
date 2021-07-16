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
#pragma once

#include <string>

#ifdef GetObject
#undef GetObject
#endif
#include "rapidjson/document.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace processors {

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

}  // namespace processors
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
