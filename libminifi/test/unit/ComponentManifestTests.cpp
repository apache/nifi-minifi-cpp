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

#undef LOAD_EXTENSIONS
#include "unit/TestBase.h"
#include "unit/Catch.h"
#include "core/Core.h"
#include "core/ConfigurableComponentImpl.h"
#include "core/controller/ControllerServiceBase.h"
#include "minifi-cpp/core/PropertyDefinition.h"
#include "core/PropertyDefinitionBuilder.h"
#include "core/Resource.h"
#include "core/state/nodes/AgentInformation.h"
#include "core/ProcessorImpl.h"

using minifi::state::response::SerializedResponseNode;

SerializedResponseNode& get(SerializedResponseNode& node, const std::string& field) {
  REQUIRE(!node.array);
  for (auto& child : node.children) {
    if (child.name == field) return child;
  }
  throw std::logic_error("No field '" + field + "'");
}

namespace test::apple {

class ExampleService : public core::controller::ControllerServiceBase {
 public:
  using ControllerServiceBase::ControllerServiceBase;

  static constexpr const char* Description = "An example service";
  static constexpr auto Properties = std::array<core::PropertyReference, 0>{};
  static constexpr bool SupportsDynamicProperties = false;
};

REGISTER_RESOURCE(ExampleService, ControllerService);

class ExampleProcessor : public core::ProcessorImpl {
 public:
  using ProcessorImpl::ProcessorImpl;

  static constexpr const char* Description = "An example processor";
  static constexpr auto ExampleProperty = core::PropertyDefinitionBuilder<>::createProperty("Example Property")
      .withDescription("An example property")
      .isRequired(false)
      .withAllowedTypes<ExampleService>()
      .build();
  static constexpr auto Properties = std::to_array<core::PropertyReference>({ExampleProperty});
  static constexpr auto Relationships = std::array<core::RelationshipDefinition, 0>{};
  static constexpr bool SupportsDynamicProperties = false;
  static constexpr bool SupportsDynamicRelationships = false;
  static constexpr core::annotation::Input InputRequirement = core::annotation::Input::INPUT_ALLOWED;
  static constexpr bool IsSingleThreaded = false;
  ADD_COMMON_VIRTUAL_FUNCTIONS_FOR_PROCESSORS

  void initialize() override {
    setSupportedProperties(Properties);
  }
};

REGISTER_RESOURCE(ExampleProcessor, Processor);

}  // namespace test::apple

TEST_CASE("Manifest indicates property type requirement") {
  const auto system_bundle_id = minifi::BundleIdentifier{.name = "minifi-system", .version = minifi::AgentBuild::VERSION};
  const auto system_components = minifi::ClassDescriptionRegistry::getClassDescriptions().at(system_bundle_id);
  auto nodes = minifi::state::response::serializeComponentManifest(system_components);
  REQUIRE(nodes.size() == 1);
  REQUIRE(nodes.at(0).name == "componentManifest");

  auto& processors = get(nodes.at(0), "processors").children;

  auto example_proc_it = std::find_if(processors.begin(), processors.end(), [&] (auto& proc) {
    return get(proc, "type").value == "test.apple.ExampleProcessor";
  });
  REQUIRE(example_proc_it != processors.end());

  auto& properties = get(*example_proc_it, "propertyDescriptors").children;

  auto prop_it = std::find_if(properties.begin(), properties.end(), [&] (auto& prop) {
    return get(prop, "name").value == "Example Property";
  });

  REQUIRE(prop_it != properties.end());

  // TODO(adebreceni): based on Property::types_ a property could accept
  //    multiple types, these would be dumped into the same object as the type of
  //    field "typeProvidedByValue" is not an array but an object
  auto& type = get(*prop_it, "typeProvidedByValue");

  REQUIRE(get(type, "type").value == "test.apple.ExampleService");
  REQUIRE(get(type, "group").value == GROUP_STR);  // fix string
  REQUIRE(get(type, "artifact").value == "minifi-system");
}

TEST_CASE("Processors do not get instantiated during manifest creation") {
  LogTestController::getInstance().setDebug<core::Processor>();

  const auto system_bundle_id = minifi::BundleIdentifier{.name = "minifi-system", .version = minifi::AgentBuild::VERSION};
  const auto system_components = minifi::ClassDescriptionRegistry::getClassDescriptions().at(system_bundle_id);
  minifi::state::response::serializeComponentManifest(system_components);

  CHECK_FALSE(LogTestController::getInstance().contains("Processor ExampleProcessor created"));
}
