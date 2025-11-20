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

#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN 1
#endif

#include "minifi-c.h"
#include "core/ClassName.h"
#include "api/utils/minifi-c-utils.h"
#include "ProcessContext.h"
#include "ProcessSession.h"
#include "FlowFile.h"
#include "minifi-cpp/core/ProcessorMetadata.h"
#include "logging/Logger.h"

namespace org::apache::nifi::minifi::api::core {

template<typename Class, typename Fn>
void useProcessorClassDescription(Fn&& fn) {
  std::vector<std::vector<MinifiStringView>> string_vector_cache;

  const auto full_name = minifi::core::className<Class>();

  std::vector<MinifiPropertyDefinition> class_properties = utils::toProperties(Class::Properties, string_vector_cache);
  std::vector<MinifiDynamicPropertyDefinition> dynamic_properties;
  for (auto& prop : Class::DynamicProperties) {
    dynamic_properties.push_back(MinifiDynamicPropertyDefinition {
      .name = utils::toStringView(prop.name),
      .value = utils::toStringView(prop.value),
      .description = utils::toStringView(prop.description),
      .supports_expression_language = prop.supports_expression_language
    });
  }
  std::vector<MinifiRelationshipDefinition> relationships;
  for (auto& rel : Class::Relationships) {
    relationships.push_back(MinifiRelationshipDefinition{
      .name = utils::toStringView(rel.name),
      .description = utils::toStringView(rel.description)
    });
  }
  std::vector<std::vector<MinifiStringView>> attribute_relationships_cache;
  std::vector<MinifiOutputAttributeDefinition> output_attributes;
  for (auto& attr : Class::OutputAttributes) {
    std::vector<MinifiStringView> rel_cache;
    for (auto& rel : attr.relationships) {
      rel_cache.push_back(utils::toStringView(rel.name));
    }
    output_attributes.push_back(MinifiOutputAttributeDefinition {
      .name = utils::toStringView(attr.name),
      .relationships_count = gsl::narrow<uint32_t>(attr.relationships.size()),
      .relationships_ptr = rel_cache.data(),
      .description = utils::toStringView(attr.description)
    });
    attribute_relationships_cache.push_back(std::move(rel_cache));
  }

  MinifiProcessorClassDefinition description{
    .full_name = utils::toStringView(full_name),
    .description = utils::toStringView(Class::Description),
    .class_properties_count = gsl::narrow<uint32_t>(class_properties.size()),
    .class_properties_ptr = class_properties.data(),
    .dynamic_properties_count = gsl::narrow<uint32_t>(dynamic_properties.size()),
    .dynamic_properties_ptr = dynamic_properties.data(),
    .class_relationships_count = gsl::narrow<uint32_t>(relationships.size()),
    .class_relationships_ptr = relationships.data(),
    .output_attributes_count = gsl::narrow<uint32_t>(output_attributes.size()),
    .output_attributes_ptr = output_attributes.data(),
    .supports_dynamic_properties = Class::SupportsDynamicProperties,
    .supports_dynamic_relationships = Class::SupportsDynamicRelationships,
    .input_requirement = utils::toInputRequirement(Class::InputRequirement),
    .is_single_threaded = Class::IsSingleThreaded,

    .callbacks = MinifiProcessorCallbacks{
      .create = [] (MinifiProcessorMetadata metadata) -> MINIFI_OWNED void* {
        return new Class{minifi::core::ProcessorMetadata{
          .uuid = minifi::utils::Identifier::parse(std::string{metadata.uuid.data, metadata.uuid.length}).value(),
          .name = std::string{metadata.name.data, metadata.name.length},
          .logger = std::make_shared<logging::Logger>(metadata.logger)
        }};
      },
      .destroy = [] (MINIFI_OWNED void* self) -> void {
        delete static_cast<Class*>(self);
      },
      .isWorkAvailable = [] (void* self) -> MinifiBool {
        return static_cast<Class*>(self)->isWorkAvailable();
      },
      .restore = [] (void* self, MINIFI_OWNED MinifiFlowFile* ff) -> void {
        static_cast<Class*>(self)->restore(std::make_shared<FlowFile>(ff));
      },
      .getTriggerWhenEmpty = [] (void* self) -> MinifiBool {
        return static_cast<Class*>(self)->getTriggerWhenEmpty();
      },
      .onTrigger = [] (void* self, MinifiProcessContext* context, MinifiProcessSession* session) -> MinifiStatus {
        ProcessContext context_wrapper(context);
        ProcessSession session_wrapper(session);
        try {
          return static_cast<Class*>(self)->onTrigger(context_wrapper, session_wrapper);
        } catch (...) {
          return MINIFI_STATUS_UNKNOWN_ERROR;
        }
      },
      .onSchedule = [] (void* self, MinifiProcessContext* context) -> MinifiStatus {
        ProcessContext context_wrapper(context);
        try {
          return static_cast<Class*>(self)->onSchedule(context_wrapper);
        } catch (...) {
          return MINIFI_STATUS_UNKNOWN_ERROR;
        }
      },
      .onUnSchedule = [] (void* self) -> void {
        static_cast<Class*>(self)->onUnSchedule();
      },
      .calculateMetrics = [] (void* self) -> MINIFI_OWNED MinifiPublishedMetrics* {
        auto metrics = static_cast<Class*>(self)->calculateMetrics();
        std::vector<MinifiStringView> names;
        std::vector<double> values;
        for (auto& [name, val] : metrics) {
          names.push_back(utils::toStringView(name));
          values.push_back(val);
        }
        return MinifiPublishedMetricsCreate(gsl::narrow<uint32_t>(metrics.size()), names.data(), values.data());
      }
    }
  };

  fn(description);
}

}  // namespace org::apache::nifi::minifi::api::core
