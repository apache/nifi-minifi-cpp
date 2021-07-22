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

#include "HeartbeatReporter.h"
#include "C2Payload.h"

#include "rapidjson/document.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace c2 {

class HeartbeatJsonSerializer {
 public:
  virtual std::string serializeJsonRootPayload(const C2Payload& payload);

 protected:
  virtual rapidjson::Value serializeJsonPayload(const C2Payload& payload, rapidjson::Document::AllocatorType& alloc);
  virtual void serializeNestedPayload(rapidjson::Value& target, const C2Payload& payload, rapidjson::Document::AllocatorType& alloc);

  virtual ~HeartbeatJsonSerializer() = default;
};

}  // namespace c2
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
