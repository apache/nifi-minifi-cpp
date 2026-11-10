// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#include "api/core/Resource.h"
#include "api/utils/minifi-c-utils.h"
#include "api/core/ProcessorImpl.h"

#define MKSOC(x) #x
#define MAKESTRING(x) MKSOC(x)  // NOLINT(cppcoreguidelines-macro-usage)

namespace minifi = org::apache::nifi::minifi;

class DummyCProcessor : public minifi::api::core::ProcessorImpl {
 public:
  using ProcessorImpl::ProcessorImpl;

  static constexpr const char* Description = "C processor that does nothing";

  static constexpr auto Relationships = std::array<minifi::core::RelationshipDefinition, 0>{};
  static constexpr auto Properties = std::array<minifi::core::PropertyReference, 0>{};
  static constexpr auto OutputAttributes = std::array<minifi::core::OutputAttributeReference, 0>{};
  static constexpr bool SupportsDynamicProperties = false;
  static constexpr bool SupportsDynamicRelationships = false;
  static constexpr minifi::core::annotation::Input InputRequirement = minifi::core::annotation::Input::INPUT_ALLOWED;
  static constexpr bool IsSingleThreaded = true;
};

CEXTENSIONAPI const uint32_t minifi_api_version = MINIFI_TEST_API_VERSION;

CEXTENSIONAPI void minifi_init_extension(minifi_extension_context* extension_context) {
  minifi_extension_definition extension_definition{
    .name = minifi::api::utils::minifiStringView(MAKESTRING(EXTENSION_NAME)),
    .version = minifi::api::utils::minifiStringView(MAKESTRING(EXTENSION_VERSION)),
    .group_name = minifi::api::utils::minifiStringView(MAKESTRING(MINIFI_EXTENSION_GROUP_NAME)),
    .deinit = nullptr,
    .user_data = nullptr
  };
  auto extension = minifi_register_extension(extension_context, &extension_definition);
  minifi::api::core::useProcessorClassDefinition<DummyCProcessor>([&] (const minifi_processor_definition& definition) {
    minifi_register_processor(extension, &definition);
  });
}
