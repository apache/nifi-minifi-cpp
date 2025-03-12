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
#include "utils/ProcessorConfigUtils.h"

#include <string>

#include "utils/expected.h"

namespace org::apache::nifi::minifi::utils {

std::string parseProperty(const core::ProcessContext& ctx, const core::PropertyReference& property, const core::FlowFile* flow_file) {
  return ctx.getProperty(property, flow_file) | utils::expect(fmt::format("Expected valid value from {}::{}", ctx.getProcessor().getName(), property.name));
}

std::optional<std::string> parseOptionalProperty(const core::ProcessContext& ctx, const core::PropertyReference& property, const core::FlowFile* flow_file) {
  return ctx.getProperty(property, flow_file) | utils::toOptional();
}

std::optional<bool> parseOptionalBoolProperty(const core::ProcessContext& ctx, const core::PropertyReference& property, const core::FlowFile* flow_file) {
  if (const auto property_str = ctx.getProperty(property, flow_file)) {
    return parsing::parseBool(*property_str) | utils::expect(fmt::format("Expected parsable bool from {}::{}", ctx.getProcessor().getName(), property.name));
  }
  return std::nullopt;
}

bool parseBoolProperty(const core::ProcessContext& ctx, const core::PropertyReference& property, const core::FlowFile* flow_file) {
  return ctx.getProperty(property, flow_file) | utils::andThen(parsing::parseBool) | utils::expect(fmt::format("Expected parsable bool from {}::{}", ctx.getProcessor().getName(), property.name));
}

std::optional<uint64_t> parseOptionalU64Property(const core::ProcessContext& ctx, const core::PropertyReference& property, const core::FlowFile* flow_file) {
  if (const auto property_str = ctx.getProperty(property, flow_file)) {
    if (property_str->empty()) {
        return std::nullopt;
    }
    return parsing::parseIntegral<uint64_t>(*property_str) | utils::expect(fmt::format("Expected parsable uint64_t from {}::{}", ctx.getProcessor().getName(), property.name));
  }

  return std::nullopt;
}

uint64_t parseU64Property(const core::ProcessContext& ctx, const core::PropertyReference& property, const core::FlowFile* flow_file) {
  return ctx.getProperty(property, flow_file) | utils::andThen(parsing::parseIntegral<uint64_t>) | utils::expect(fmt::format("Expected parsable uint64_t from {}::{}", ctx.getProcessor().getName(), property.name));
}

std::optional<int64_t> parseOptionalI64Property(const core::ProcessContext& ctx, const core::PropertyReference& property, const core::FlowFile* flow_file) {
  if (const auto property_str = ctx.getProperty(property, flow_file)) {
    if (property_str->empty()) {
      return std::nullopt;
    }
    return parsing::parseIntegral<int64_t>(*property_str) | utils::expect(fmt::format("Expected parsable int64_t from {}::{}", ctx.getProcessor().getName(), property.name));
  }

  return std::nullopt;
}

int64_t parseI64Property(const core::ProcessContext& ctx, const core::PropertyReference& property, const core::FlowFile* flow_file) {
  return ctx.getProperty(property, flow_file) | utils::andThen(parsing::parseIntegral<int64_t>) | utils::expect(fmt::format("Expected parsable int64_t from {}::{}", ctx.getProcessor().getName(), property.name));
}

std::optional<std::chrono::milliseconds> parseOptionalMsProperty(const core::ProcessContext& ctx, const core::PropertyReference& property, const core::FlowFile* flow_file) {
  if (const auto property_str = ctx.getProperty(property, flow_file)) {
    if (property_str->empty()) {
      return std::nullopt;
    }
    return parsing::parseDuration(*property_str) | utils::expect(fmt::format("Expected parsable duration from {}::{}", ctx.getProcessor().getName(), property.name));
  }

  return std::nullopt;
}

std::chrono::milliseconds parseMsProperty(const core::ProcessContext& ctx, const core::PropertyReference& property, const core::FlowFile* flow_file) {
  return ctx.getProperty(property, flow_file) | utils::andThen(parsing::parseDuration<std::chrono::milliseconds>) | utils::expect(fmt::format("Expected parsable duration from {}::{}", ctx.getProcessor().getName(), property.name));
}

std::optional<uint64_t> parseOptionalDataSizeProperty(const core::ProcessContext& ctx, const core::PropertyReference& property, const core::FlowFile* flow_file) {
  if (const auto property_str = ctx.getProperty(property, flow_file)) {
    if (property_str->empty()) {
      return std::nullopt;
    }
    return parsing::parseDataSize(*property_str) | utils::expect(fmt::format("Expected parsable data size from {}::{}", ctx.getProcessor().getName(), property.name));
  }

  return std::nullopt;
}

uint64_t parseDataSizeProperty(const core::ProcessContext& ctx, const core::PropertyReference& property, const core::FlowFile* flow_file) {
  return ctx.getProperty(property, flow_file) | utils::andThen(parsing::parseDataSize) | utils::expect(fmt::format("Expected parsable data size from {}::{}", ctx.getProcessor().getName(), property.name));
}


}  // namespace org::apache::nifi::minifi::utils
