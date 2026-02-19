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

#include "core/ClassName.h"
#include "minifi-cpp/core/ProcessContext.h"
#include "minifi-cpp/core/PropertyValidator.h"
#include "utils/Enum.h"
#include "utils/expected.h"
#include "utils/OptionalUtils.h"
#include "minifi-cpp/Exception.h"

namespace org::apache::nifi::minifi::utils {

inline std::string parseProperty(const core::ProcessContext& ctx, const core::PropertyReference& property, const core::FlowFile* flow_file = nullptr) {
  return ctx.getProperty(property.name, flow_file)
      | orThrow(fmt::format("Expected valid value from \"{}\"", property.name));
}

inline bool parseBoolProperty(const core::ProcessContext& ctx, const core::PropertyReference& property, const core::FlowFile* flow_file = nullptr) {
  return ctx.getProperty(property.name, flow_file)
      | andThen(parsing::parseBool)
      | orThrow(fmt::format("Expected parsable bool from \"{}\"", property.name));
}

inline uint64_t parseU64Property(const core::ProcessContext& ctx, const core::PropertyReference& property, const core::FlowFile* flow_file = nullptr) {
  return ctx.getProperty(property.name, flow_file)
      | andThen(parsing::parseIntegral<uint64_t>)
      | orThrow(fmt::format("Expected parsable uint64_t from \"{}\"", property.name));
}

inline int64_t parseI64Property(const core::ProcessContext& ctx, const core::PropertyReference& property, const core::FlowFile* flow_file = nullptr) {
  return ctx.getProperty(property.name, flow_file)
      | andThen(parsing::parseIntegral<int64_t>)
      | orThrow(fmt::format("Expected parsable int64_t from \"{}\"", property.name));
}

inline std::chrono::milliseconds parseDurationProperty(const core::ProcessContext& ctx, const core::PropertyReference& property, const core::FlowFile* flow_file = nullptr) {
  return ctx.getProperty(property.name, flow_file)
      | andThen(parsing::parseDuration<std::chrono::milliseconds>)
      | orThrow(fmt::format("Expected parsable duration from \"{}\"", property.name));
}

inline uint64_t parseDataSizeProperty(const core::ProcessContext& ctx, const core::PropertyReference& property, const core::FlowFile* flow_file = nullptr) {
  return ctx.getProperty(property.name, flow_file)
      | andThen(parsing::parseDataSize)
      | orThrow(fmt::format("Expected parsable data size from \"{}\"", property.name));
}

inline std::optional<std::string> parseOptionalProperty(const core::ProcessContext& ctx, const core::PropertyReference& property, const core::FlowFile* flow_file = nullptr) {
  return ctx.getProperty(property.name, flow_file)
      | utils::toOptional();
}

inline std::optional<bool> parseOptionalBoolProperty(const core::ProcessContext& ctx, const core::PropertyReference& property, const core::FlowFile* flow_file = nullptr) {
  if (const auto property_str = ctx.getProperty(property.name, flow_file)) {
    return parsing::parseBool(*property_str)
        | utils::orThrow(fmt::format("Expected parsable bool from \"{}\"", property.name));
  }
  return std::nullopt;
}

inline std::optional<uint64_t> parseOptionalU64Property(const core::ProcessContext& ctx, const core::PropertyReference& property, const core::FlowFile* flow_file = nullptr) {
  if (const auto property_str = ctx.getProperty(property.name, flow_file)) {
    if (property_str->empty()) {
      return std::nullopt;
    }
    return parsing::parseIntegral<uint64_t>(*property_str)
        | utils::orThrow(fmt::format("Expected parsable uint64_t from \"{}\"", property.name));
  }

  return std::nullopt;
}

inline std::optional<int64_t> parseOptionalI64Property(const core::ProcessContext& ctx, const core::PropertyReference& property, const core::FlowFile* flow_file = nullptr) {
  if (const auto property_str = ctx.getProperty(property.name, flow_file)) {
    if (property_str->empty()) {
      return std::nullopt;
    }
    return parsing::parseIntegral<int64_t>(*property_str)
        | utils::orThrow(fmt::format("Expected parsable int64_t from \"{}\"", property.name));
  }

  return std::nullopt;
}

inline std::optional<std::chrono::milliseconds> parseOptionalDurationProperty(const core::ProcessContext& ctx, const core::PropertyReference& property, const core::FlowFile* flow_file = nullptr) {
  if (const auto property_str = ctx.getProperty(property.name, flow_file)) {
    if (property_str->empty()) {
      return std::nullopt;
    }
    return parsing::parseDuration(*property_str)
        | utils::orThrow(fmt::format("Expected parsable duration from \"{}\"", property.name));
  }

  return std::nullopt;
}

inline std::optional<uint64_t> parseOptionalDataSizeProperty(const core::ProcessContext& ctx, const core::PropertyReference& property, const core::FlowFile* flow_file = nullptr) {
  if (const auto property_str = ctx.getProperty(property.name, flow_file)) {
    if (property_str->empty()) {
      return std::nullopt;
    }
    return parsing::parseDataSize(*property_str)
        | utils::orThrow(fmt::format("Expected parsable data size from \"{}\"", property.name));
  }

  return std::nullopt;
}

inline std::optional<float> parseOptionalFloatProperty(const core::ProcessContext& ctx, const core::PropertyReference& property, const core::FlowFile* flow_file = nullptr) {
  if (const auto property_str = ctx.getProperty(property.name, flow_file)) {
    if (property_str->empty()) {
      return std::nullopt;
    }
    return parsing::parseFloat(*property_str)
        | utils::orThrow(fmt::format("Expected parsable float from \"{}\"", property.name));
  }
  return std::nullopt;
}

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
std::shared_ptr<ControllerServiceType> parseOptionalControllerService(const core::ProcessContext& context, const core::PropertyReference& prop, const utils::Identifier& processor_uuid) {
  const auto controller_service_name = context.getProperty(prop.name);
  if (!controller_service_name || controller_service_name->empty()) {
    return nullptr;
  }

  const std::shared_ptr<core::controller::ControllerService> service = context.getControllerService(*controller_service_name, processor_uuid);
  if (!service) {
    throw Exception(PROCESS_SCHEDULE_EXCEPTION, fmt::format("Controller service '{}' = '{}' not found", prop.name, *controller_service_name));
  }

  auto typed_controller_service = std::dynamic_pointer_cast<ControllerServiceType>(service);
  if (!typed_controller_service) {
    throw Exception(PROCESS_SCHEDULE_EXCEPTION, fmt::format("Controller service '{}' = '{}' is not of type {}", prop.name, *controller_service_name, core::className<ControllerServiceType>()));
  }

  return typed_controller_service;
}

template<typename ControllerServiceType>
gsl::not_null<std::shared_ptr<ControllerServiceType>> parseControllerService(const core::ProcessContext& context, const core::PropertyReference& prop, const utils::Identifier& processor_uuid) {
  auto controller_service = parseOptionalControllerService<ControllerServiceType>(context, prop, processor_uuid);
  if (!controller_service) {
    throw Exception(PROCESS_SCHEDULE_EXCEPTION, fmt::format("Required controller service property '{}' is missing", prop.name));
  }
  return gsl::make_not_null(controller_service);
}
}  // namespace org::apache::nifi::minifi::utils
