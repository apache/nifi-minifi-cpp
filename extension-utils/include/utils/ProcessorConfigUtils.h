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
#include "minifi-cpp/core/PropertyValidator.h"
#include "utils/Enum.h"
#include "utils/PropertyErrors.h"

namespace org::apache::nifi::minifi::utils {

inline std::string parseProperty(const core::ProcessContext& ctx, const core::PropertyReference& property, const core::FlowFile* flow_file = nullptr) {
  return ctx.getProperty(property.name, flow_file) | orThrow(fmt::format("Expected valid value from {}::{}", ctx.getProcessor().getName(), property.name));
}

inline bool parseBoolProperty(const core::ProcessContext& ctx, const core::PropertyReference& property, const core::FlowFile* flow_file = nullptr) {
  return ctx.getProperty(property.name, flow_file) | andThen(parsing::parseBool) | orThrow(fmt::format("Expected parsable bool from {}::{}", ctx.getProcessor().getName(), property.name));
}

inline uint64_t parseU64Property(const core::ProcessContext& ctx, const core::PropertyReference& property, const core::FlowFile* flow_file = nullptr) {
  return ctx.getProperty(property.name, flow_file) | andThen(parsing::parseIntegral<uint64_t>) | orThrow(fmt::format("Expected parsable uint64_t from {}::{}", ctx.getProcessor().getName(), property.name));
}

inline int64_t parseI64Property(const core::ProcessContext& ctx, const core::PropertyReference& property, const core::FlowFile* flow_file = nullptr) {
  return ctx.getProperty(property.name, flow_file) | andThen(parsing::parseIntegral<int64_t>) | orThrow(fmt::format("Expected parsable int64_t from {}::{}", ctx.getProcessor().getName(), property.name));
}

inline std::chrono::milliseconds parseDurationProperty(const core::ProcessContext& ctx, const core::PropertyReference& property, const core::FlowFile* flow_file = nullptr) {
  return ctx.getProperty(property.name, flow_file) | andThen(parsing::parseDuration<std::chrono::milliseconds>) | orThrow(fmt::format("Expected parsable duration from {}::{}", ctx.getProcessor().getName(), property.name));
}

inline uint64_t parseDataSizeProperty(const core::ProcessContext& ctx, const core::PropertyReference& property, const core::FlowFile* flow_file = nullptr) {
  return ctx.getProperty(property.name, flow_file) | andThen(parsing::parseDataSize) | orThrow(fmt::format("Expected parsable data size from {}::{}", ctx.getProcessor().getName(), property.name));
}

inline std::optional<std::string> parseOptionalProperty(const core::ProcessContext& ctx, const core::PropertyReference& property, const core::FlowFile* flow_file = nullptr) {
  return ctx.getProperty(property.name, flow_file) | utils::toOptional();
}

inline std::optional<bool> parseOptionalBoolProperty(const core::ProcessContext& ctx, const core::PropertyReference& property, const core::FlowFile* flow_file = nullptr) {
  const auto property_str = ctx.getProperty(property.name, flow_file);
  if (property_str && property_str->empty()) {
    return std::nullopt;
  }
  if (!property_str && property_str.error() == core::PropertyErrorCode::PropertyNotSet) {
    return std::nullopt;
  }

  return property_str
      | andThen(parsing::parseBool)
      | orThrow(fmt::format("Expected parsable bool from {}::{}", ctx.getProcessor().getName(), property.name));
}

inline std::optional<uint64_t> parseOptionalU64Property(const core::ProcessContext& ctx, const core::PropertyReference& property, const core::FlowFile* flow_file = nullptr) {
  const auto property_str = ctx.getProperty(property.name, flow_file);
  if (property_str && property_str->empty()) {
    return std::nullopt;
  }
  if (!property_str && property_str.error() == core::PropertyErrorCode::PropertyNotSet) {
    return std::nullopt;
  }

  return property_str
      | andThen(parsing::parseIntegral<uint64_t>)
      | orThrow(fmt::format("Expected parsable uint64_t from {}::{}", ctx.getProcessor().getName(), property.name));
}

inline std::optional<int64_t> parseOptionalI64Property(const core::ProcessContext& ctx, const core::PropertyReference& property, const core::FlowFile* flow_file = nullptr) {
  const auto property_str = ctx.getProperty(property.name, flow_file);
  if (property_str && property_str->empty()) {
    return std::nullopt;
  }
  if (!property_str && property_str.error() == core::PropertyErrorCode::PropertyNotSet) {
    return std::nullopt;
  }

  return property_str
      | andThen(parsing::parseIntegral<int64_t>)
      | orThrow(fmt::format("Expected parsable int64_t from {}::{}", ctx.getProcessor().getName(), property.name));
}

inline std::optional<std::chrono::milliseconds> parseOptionalDurationProperty(const core::ProcessContext& ctx, const core::PropertyReference& property, const core::FlowFile* flow_file = nullptr) {
  const auto property_str = ctx.getProperty(property.name, flow_file);
  if (property_str && property_str->empty()) {
    return std::nullopt;
  }
  if (!property_str && property_str.error() == core::PropertyErrorCode::PropertyNotSet) {
    return std::nullopt;
  }

  return property_str
      | andThen(parsing::parseDuration<std::chrono::milliseconds>)
      | orThrow(fmt::format("Expected parsable duration from {}::{}", ctx.getProcessor().getName(), property.name));
}

inline std::optional<uint64_t> parseOptionalDataSizeProperty(const core::ProcessContext& ctx, const core::PropertyReference& property, const core::FlowFile* flow_file = nullptr) {
  const auto property_str = ctx.getProperty(property.name, flow_file);
  if (property_str && property_str->empty()) {
    return std::nullopt;
  }
  if (!property_str && property_str.error() == core::PropertyErrorCode::PropertyNotSet) {
    return std::nullopt;
  }

  return property_str
      | andThen(parsing::parseDataSize)
      | orThrow(fmt::format("Expected parsable data size from {}::{}", ctx.getProcessor().getName(), property.name));
}

template<typename T>
T parseEnumProperty(const core::ProcessContext& context, const core::PropertyReference& prop, const core::FlowFile* flow_file = nullptr) {
  return context.getProperty(prop.name, flow_file) | andThen(parsing::parseEnum<T>) | orThrow(fmt::format("Expected valid enum from {}::{}", context.getProcessor().getName(), prop.name));
}

template<typename T>
std::optional<T> parseOptionalEnumProperty(const core::ProcessContext& ctx, const core::PropertyReference& property, const core::FlowFile* flow_file = nullptr) {
  const auto property_str = ctx.getProperty(property.name, flow_file);
  if (property_str && property_str->empty()) {
    return std::nullopt;
  }
  if (!property_str && property_str.error() == core::PropertyErrorCode::PropertyNotSet) {
    return std::nullopt;
  }
  return property_str
    | andThen(parsing::parseEnum<T>)
    | orThrow(fmt::format("Expected valid enum from {}::{}", ctx.getProcessor().getName(), property.name));
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
