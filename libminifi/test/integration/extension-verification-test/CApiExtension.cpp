/**
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

CEXTENSIONAPI const uint32_t MinifiApiVersion = MINIFI_TEST_API_VERSION;

CEXTENSIONAPI void MinifiInitExtension(MinifiExtensionContext* extension_context) {
  MinifiExtensionDefinition extension_definition{
    .name = minifi::api::utils::toStringView(MAKESTRING(EXTENSION_NAME)),
    .version = minifi::api::utils::toStringView(MAKESTRING(EXTENSION_VERSION)),
    .deinit = nullptr,
    .user_data = nullptr
  };
  auto extension = MinifiRegisterExtension(extension_context, &extension_definition);
  minifi::api::core::useProcessorClassDefinition<DummyCProcessor>([&] (const MinifiProcessorClassDefinition& definition) {
    MinifiRegisterProcessor(extension, &definition);
  });
}
