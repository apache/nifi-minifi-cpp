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

#include <optional>
#include <set>
#include <string>
#include <vector>

#include "core/ProcessContext.h"
#include "core/PropertyType.h"
#include "utils/Enum.h"

namespace org::apache::nifi::minifi::utils {

std::optional<std::string> parseOptionalProperty(const core::ProcessContext& ctx, const core::PropertyReference& property, const core::FlowFile* flow_file = nullptr);
std::string parseProperty(const core::ProcessContext& ctx, const core::PropertyReference& property, const core::FlowFile* flow_file = nullptr);

std::optional<bool> parseOptionalBoolProperty(const core::ProcessContext& ctx, const core::PropertyReference& property, const core::FlowFile* flow_file = nullptr);
bool parseBoolProperty(const core::ProcessContext& ctx, const core::PropertyReference& property, const core::FlowFile* flow_file = nullptr);

std::optional<uint64_t> parseOptionalU64Property(const core::ProcessContext& ctx, const core::PropertyReference& property, const core::FlowFile* flow_file = nullptr);
uint64_t parseU64Property(const core::ProcessContext& ctx, const core::PropertyReference& property, const core::FlowFile* flow_file = nullptr);

std::optional<int64_t> parseOptionalI64Property(const core::ProcessContext& ctx, const core::PropertyReference& property, const core::FlowFile* flow_file = nullptr);
int64_t parseI64Property(const core::ProcessContext& ctx, const core::PropertyReference& property, const core::FlowFile* flow_file = nullptr);

std::optional<std::chrono::milliseconds> parseOptionalDurationProperty(const core::ProcessContext& ctx, const core::PropertyReference& property, const core::FlowFile* flow_file = nullptr);
std::chrono::milliseconds parseDurationProperty(const core::ProcessContext& ctx, const core::PropertyReference& property, const core::FlowFile* flow_file = nullptr);

std::optional<uint64_t> parseOptionalDataSizeProperty(const core::ProcessContext& ctx, const core::PropertyReference& property, const core::FlowFile* flow_file = nullptr);
uint64_t parseDataSizeProperty(const core::ProcessContext& ctx, const core::PropertyReference& property, const core::FlowFile* flow_file = nullptr);

template<typename T>
T parseEnumProperty(const core::ProcessContext& context, const core::PropertyReference& prop, const core::FlowFile* flow_file = nullptr) {
  const auto enum_str = context.getProperty(prop.name, flow_file);
  if (!enum_str) {
    throw Exception(PROCESS_SCHEDULE_EXCEPTION, "Property '" + std::string(prop.name) + "' is missing");
  }
  auto result = magic_enum::enum_cast<T>(*enum_str);
  if (!result) {
    throw Exception(PROCESS_SCHEDULE_EXCEPTION, "Property '" + std::string(prop.name) + "' has invalid value: '" + *enum_str + "'");
  }
  return result.value();
}

template<typename T>
std::optional<T> parseOptionalEnumProperty(const core::ProcessContext& context, const core::PropertyReference& prop) {
  const auto enum_str = context.getProperty(prop.name);

  if (!enum_str) {
    return std::nullopt;
  }
  auto result = magic_enum::enum_cast<T>(*enum_str);
  if (!result) {
    throw Exception(PROCESS_SCHEDULE_EXCEPTION, "Property '" + std::string(prop.name) + "' has invalid value: '" + *enum_str + "'");
  }
  return result.value();
}

template<typename ControllerServiceType>
std::optional<std::shared_ptr<ControllerServiceType>> parseOptionalControllerService(const core::ProcessContext& context, const core::PropertyReference& prop, const utils::Identifier& processor_uuid) {
  const auto controller_service_name = context.getProperty(prop.name);
  if (!controller_service_name) {
    return std::nullopt;
  }

  const std::shared_ptr<core::controller::ControllerService> service = context.getControllerService(*controller_service_name, processor_uuid);
  if (!service) {
    return std::nullopt;
  }

  auto typed_controller_service = std::dynamic_pointer_cast<ControllerServiceType>(service);
  if (!typed_controller_service) {
    return std::nullopt;
  }

  return typed_controller_service;
}

}  // namespace org::apache::nifi::minifi::utils
