/**
 * @file RouteOnAttribute.h
 * RouteOnAttribute class declaration
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
#ifndef EXTENSIONS_STANDARD_PROCESSORS_PROCESSORS_ROUTEONATTRIBUTE_H_
#define EXTENSIONS_STANDARD_PROCESSORS_PROCESSORS_ROUTEONATTRIBUTE_H_

#include <map>
#include <memory>
#include <string>
#include <utility>

#include "FlowFileRecord.h"
#include "core/Processor.h"
#include "core/ProcessSession.h"
#include "core/Core.h"
#include "core/logging/LoggerConfiguration.h"

namespace org::apache::nifi::minifi::processors {

class RouteOnAttribute : public core::Processor {
 public:
  explicit RouteOnAttribute(std::string name, const utils::Identifier& uuid = {})
      : core::Processor(std::move(name), uuid) {
  }

  EXTENSIONAPI static constexpr const char* Description = "Routes FlowFiles based on their Attributes using the Attribute Expression Language.";

  static auto properties() { return std::array<core::Property, 0>{}; }

  EXTENSIONAPI static const core::Relationship Unmatched;
  EXTENSIONAPI static const core::Relationship Failure;
  static auto relationships() { return std::array{Unmatched, Failure}; }

  EXTENSIONAPI static constexpr bool SupportsDynamicProperties = true;
  EXTENSIONAPI static constexpr bool SupportsDynamicRelationships = true;
  EXTENSIONAPI static constexpr core::annotation::Input InputRequirement = core::annotation::Input::INPUT_REQUIRED;
  EXTENSIONAPI static constexpr bool IsSingleThreaded = false;

  ADD_COMMON_VIRTUAL_FUNCTIONS_FOR_PROCESSORS

  void onDynamicPropertyModified(const core::Property &orig_property, const core::Property &new_property) override;
  void onTrigger(core::ProcessContext *context, core::ProcessSession *session) override;
  void initialize() override;

 private:
  std::shared_ptr<core::logging::Logger> logger_ = core::logging::LoggerFactory<RouteOnAttribute>::getLogger();
  std::map<std::string, core::Property> route_properties_;
  std::map<std::string, core::Relationship> route_rels_;
};

}  // namespace org::apache::nifi::minifi::processors

#endif  // EXTENSIONS_STANDARD_PROCESSORS_PROCESSORS_ROUTEONATTRIBUTE_H_
