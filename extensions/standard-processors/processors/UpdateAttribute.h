/**
 * @file UpdateAttribute.h
 * UpdateAttribute class declaration
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

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "FlowFileRecord.h"
#include "core/Processor.h"
#include "core/ProcessSession.h"
#include "core/Core.h"
#include "core/logging/LoggerConfiguration.h"

namespace org::apache::nifi::minifi {

namespace controllers {
class AttributeProviderService;
}

namespace processors {

class UpdateAttribute : public core::Processor {
 public:
  explicit UpdateAttribute(const std::string& name, const utils::Identifier& uuid = {})
          : core::Processor(name, uuid) {
  }

  EXTENSIONAPI static constexpr const char* Description = "This processor updates the attributes of a FlowFile using properties that are added by the user. "
      "This allows you to set default attribute changes that affect every FlowFile going through the processor, equivalent to the \"basic\" usage in Apache NiFi.";

  EXTENSIONAPI static const core::Property AttributeProviderService;

  static auto properties() { return std::array{AttributeProviderService}; }

  EXTENSIONAPI static const core::Relationship Success;
  EXTENSIONAPI static const core::Relationship Failure;

  static auto relationships() { return std::array{Success, Failure}; }

  EXTENSIONAPI static constexpr bool SupportsDynamicProperties = true;
  EXTENSIONAPI static constexpr bool SupportsDynamicRelationships = false;
  EXTENSIONAPI static constexpr core::annotation::Input InputRequirement = core::annotation::Input::INPUT_REQUIRED;
  EXTENSIONAPI static constexpr bool IsSingleThreaded = false;

  ADD_COMMON_VIRTUAL_FUNCTIONS_FOR_PROCESSORS

  void onSchedule(core::ProcessContext* context, core::ProcessSessionFactory* sessionFactory) override;

  void onTrigger(core::ProcessContext* context, core::ProcessSession* session) override;

  void initialize() override;

 private:
  std::unordered_map<std::string, std::string> getAttributeMap() const;

  std::shared_ptr<core::logging::Logger> logger_ = core::logging::LoggerFactory<UpdateAttribute>::getLogger();
  std::vector<core::Property> attributes_;
  controllers::AttributeProviderService* attribute_provider_service_ = nullptr;
};

}  // namespace processors
}  // namespace org::apache::nifi::minifi
