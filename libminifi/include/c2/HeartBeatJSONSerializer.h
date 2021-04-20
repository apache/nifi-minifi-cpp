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

#include "HeartBeatReporter.h"
#include "C2Payload.h"
#include "rapidjson/document.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace c2 {

class HeartBeatJSONSerializer {
 public:
  virtual void serializeNestedPayload(rapidjson::Value& target, const C2Payload& payload, rapidjson::Document::AllocatorType& alloc);
  virtual std::string serializeJsonRootPayload(const C2Payload& payload);
  virtual rapidjson::Value serializeJsonPayload(const C2Payload& payload, rapidjson::Document::AllocatorType& alloc);
  static rapidjson::Value serializeConnectionQueues(const C2Payload& payload, std::string& label, rapidjson::Document::AllocatorType& alloc);
  static void setJsonStr(const std::string& key, const state::response::ValueNode& value, rapidjson::Value& parent, rapidjson::Document::AllocatorType& alloc);
  static rapidjson::Value getStringValue(const std::string& value, rapidjson::Document::AllocatorType& alloc);
  static void mergePayloadContent(rapidjson::Value& target, const C2Payload& payload, rapidjson::Document::AllocatorType& alloc);
  static std::string getOperation(const C2Payload& payload);

  virtual ~HeartBeatJSONSerializer() = default;
};

}  // namespace c2
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
