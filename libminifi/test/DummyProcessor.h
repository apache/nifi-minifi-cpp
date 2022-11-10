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
#include <string>
#include <utility>

#include "core/Processor.h"
#include "agent/agent_docs.h"

namespace org::apache::nifi::minifi::test {

class DummyProcessor : public minifi::core::Processor {
  using minifi::core::Processor::Processor;

 public:
  DummyProcessor(std::string name, const minifi::utils::Identifier& uuid) : Processor(std::move(name), uuid) {}
  explicit DummyProcessor(std::string name) : Processor(std::move(name)) {}
  static constexpr const char* Description = "A processor that does nothing.";
  static auto properties() { return std::array<core::Property, 0>{}; }
  static auto relationships() { return std::array<core::Relationship, 0>{}; }
  static constexpr bool SupportsDynamicProperties = false;
  static constexpr bool SupportsDynamicRelationships = false;
  static constexpr core::annotation::Input InputRequirement = core::annotation::Input::INPUT_ALLOWED;
  static constexpr bool IsSingleThreaded = false;
  ADD_COMMON_VIRTUAL_FUNCTIONS_FOR_PROCESSORS
};

}  // namespace org::apache::nifi::minifi::test
