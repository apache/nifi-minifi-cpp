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

#include <array>
#include <string_view>

namespace org::apache::nifi::minifi::core {

class IFlowFile {
 public:
  virtual ~IFlowFile() = default;
};

struct SpecialFlowAttribute {
  static constexpr std::string_view PATH = "path";
  static constexpr std::string_view ABSOLUTE_PATH = "absolute.path";
  static constexpr std::string_view FILENAME = "filename";
  static constexpr std::string_view UUID = "uuid";
  static constexpr std::string_view priority = "priority";
  static constexpr std::string_view MIME_TYPE = "mime.type";
  static constexpr std::string_view DISCARD_REASON = "discard.reason";
  static constexpr std::string_view ALTERNATE_IDENTIFIER = "alternate.identifier";
  static constexpr std::string_view FLOW_ID = "flow.id";

  static constexpr std::array<std::string_view, 9> getSpecialFlowAttributes() {
    return {PATH, ABSOLUTE_PATH, FILENAME, UUID, priority, MIME_TYPE, DISCARD_REASON, ALTERNATE_IDENTIFIER, FLOW_ID};
  }
};

}  // namespace org::apache::nifi::minifi::core
