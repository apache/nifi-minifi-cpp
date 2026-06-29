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

#include "ControllerServiceContext.h"
#include "FlowFile.h"
#include "ProcessContext.h"
#include "ProcessSession.h"
#include "api/utils/minifi-c-utils.h"
#include "core/ClassName.h"
#include "logging/Logger.h"
#include "minifi-api.h"
#include "minifi-cpp/core/ControllerServiceMetadata.h"
#include "minifi-cpp/core/ProcessorMetadata.h"

namespace org::apache::nifi::minifi::api::core {

template<typename Class, typename Fn>
void useProcessorClassDefinition(Fn&& fn) {
  std::vector<std::vector<minifi_string_view>> string_vector_cache;

  const auto full_name = minifi::core::className<Class>();

  std::vector<minifi_property_definition> class_properties = utils::toProperties(Class::Properties, string_vector_cache);
  std::vector<minifi_dynamic_property_definition> dynamic_properties;
  for (auto& prop : Class::DynamicProperties) {
    dynamic_properties.push_back(minifi_dynamic_property_definition {
      .name = utils::minifiStringView(prop.name),
      .value = utils::minifiStringView(prop.value),
      .description = utils::minifiStringView(prop.description),
      .supports_expression_language = prop.supports_expression_language
    });
  }
  std::vector<minifi_relationship_definition> relationships;
  for (auto& rel : Class::Relationships) {
    relationships.push_back(minifi_relationship_definition{
      .name = utils::minifiStringView(rel.name),
      .description = utils::minifiStringView(rel.description)
    });
  }
  std::vector<std::vector<minifi_string_view>> attribute_relationships_cache;
  std::vector<minifi_output_attribute_definition> output_attributes;
  for (auto& attr : Class::OutputAttributes) {
    std::vector<minifi_string_view> rel_cache;
    for (auto& rel : attr.relationships) {
      rel_cache.push_back(utils::minifiStringView(rel.name));
    }
    output_attributes.push_back(minifi_output_attribute_definition {
      .name = utils::minifiStringView(attr.name),
      .relationships_count = gsl::narrow<uint32_t>(attr.relationships.size()),
      .relationships_ptr = rel_cache.data(),
      .description = utils::minifiStringView(attr.description)
    });
    attribute_relationships_cache.push_back(std::move(rel_cache));
  }

  minifi_processor_class_definition definition{
    .full_name = utils::minifiStringView(full_name),
    .description = utils::minifiStringView(Class::Description),
    .properties_count = gsl::narrow<uint32_t>(class_properties.size()),
    .properties_ptr = class_properties.data(),
    .dynamic_properties_count = gsl::narrow<uint32_t>(dynamic_properties.size()),
    .dynamic_properties_ptr = dynamic_properties.data(),
    .relationships_count = gsl::narrow<uint32_t>(relationships.size()),
    .relationships_ptr = relationships.data(),
    .output_attributes_count = gsl::narrow<uint32_t>(output_attributes.size()),
    .output_attributes_ptr = output_attributes.data(),
    .supports_dynamic_properties = Class::SupportsDynamicProperties,
    .supports_dynamic_relationships = Class::SupportsDynamicRelationships,
    .input_requirement = utils::toInputRequirement(Class::InputRequirement),
    .is_single_threaded = Class::IsSingleThreaded,

    .callbacks = minifi_processor_callbacks{
      .create = [] (minifi_processor_metadata metadata) -> MINIFI_OWNED void* {
        try {
          return new Class{minifi::core::ProcessorMetadata{
              .uuid = minifi::utils::Identifier::parse(std::string{metadata.uuid.data, metadata.uuid.length}).value(),
              .name = std::string{metadata.name.data, metadata.name.length},
              .logger = std::make_shared<logging::CffiLogger>(metadata.logger)}};
        } catch (...) { return nullptr; }
      },
      .destroy = [] (MINIFI_OWNED void* self) -> void {
        delete static_cast<Class*>(self);
      },
      .trigger = [] (void* self, minifi_process_context* context, minifi_process_session* session) -> minifi_status {
        CffiProcessContext context_wrapper(context);
        CffiProcessSession session_wrapper(session);
        try {
          return static_cast<Class*>(self)->onTrigger(context_wrapper, session_wrapper);
        } catch (...) {
          return MINIFI_STATUS_UNKNOWN_ERROR;
        }
      },
      .schedule = [] (void* self, minifi_process_context* context) -> minifi_status {
        CffiProcessContext context_wrapper(context);
        try {
          return static_cast<Class*>(self)->onSchedule(context_wrapper);
        } catch (...) {
          return MINIFI_STATUS_UNKNOWN_ERROR;
        }
      },
      .unschedule = [] (void* self) -> void {
        try {
          static_cast<Class*>(self)->onUnSchedule();
        } catch (...) {}
      }
    }
  };

  fn(definition);
}

template<typename Class, typename Fn>
void useControllerServiceClassDefinition(Fn&& fn) {
  std::vector<std::vector<minifi_string_view>> string_vector_cache;

  const auto full_name = minifi::core::className<Class>();

  std::vector<minifi_property_definition> class_properties = utils::toProperties(Class::Properties, string_vector_cache);
  std::vector<minifi_string_view> provided_interfaces;
  if constexpr (requires { Class::ProvidedInterfaces; }) {
    provided_interfaces.reserve(Class::ProvidedInterfaces.size());
    for (const auto& iface : Class::ProvidedInterfaces) {
      provided_interfaces.push_back(utils::minifiStringView(iface.name));
    }
  }

  minifi_controller_service_class_definition definition{.full_name = utils::minifiStringView(full_name),
      .description = utils::minifiStringView(Class::Description),
      .properties_count = gsl::narrow<uint32_t>(class_properties.size()),
      .properties_ptr = class_properties.data(),

      .provided_interfaces_count = gsl::narrow<uint32_t>(provided_interfaces.size()),
      .provided_interfaces_ptr = provided_interfaces.data(),

      .callbacks = minifi_controller_service_callbacks{
          .create = [](minifi_controller_service_metadata metadata) -> MINIFI_OWNED void* {
            try {
              return new Class{minifi::core::ControllerServiceMetadata{
                  .uuid = minifi::utils::Identifier::parse(std::string{metadata.uuid.data, metadata.uuid.length}).value(),
                  .name = std::string{metadata.name.data, metadata.name.length},
                  .logger = std::make_shared<logging::CffiLogger>(metadata.logger)}};
            } catch (...) { return nullptr; }
          },
          .destroy = [](MINIFI_OWNED void* self) -> void { delete static_cast<Class*>(self); },
          .enable = [](void* self, minifi_controller_service_context* context) -> minifi_status {
            ControllerServiceContext context_wrapper(context);
            try {
              return static_cast<Class*>(self)->enable(context_wrapper);
            } catch (...) { return MINIFI_STATUS_UNKNOWN_ERROR; }
          },
          .disable = [](void* self) -> void {
            try {
              static_cast<Class*>(self)->disable();
            } catch (...) {}
          },
          .get_interface = [](void* self, minifi_string_view interface_name) -> void* {
            try {
              if constexpr (requires { Class::ProvidedInterfaces; }) {
                const std::string_view name_view{interface_name.data, interface_name.length};
                for (const auto& iface : Class::ProvidedInterfaces) {
                  if (iface.name == name_view) {
                    return iface.cast(self);
                  }
                }
              }
              return nullptr;
            } catch (...) { return nullptr; }
            }
      }};

  fn(definition);
}

template <typename... Processors>
void registerProcessors(minifi_extension* extension) {
  (core::useProcessorClassDefinition<Processors>([&](const minifi_processor_class_definition& definition) {
      minifi_register_processor(extension, &definition);
  }), ...);
}

template <typename... ControllerServices>
void registerControllerServices(minifi_extension* extension) {
  (core::useControllerServiceClassDefinition<ControllerServices>([&](const minifi_controller_service_class_definition& definition) {
      minifi_register_controller_service(extension, &definition);
  }), ...);
}

}  // namespace org::apache::nifi::minifi::api::core
