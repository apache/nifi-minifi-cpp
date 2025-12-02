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

#define EXTENSION_LIST "*minifi-system*, *minifi-standard-processors*"  // NOLINT(cppcoreguidelines-macro-usage)

#include <memory>

#include "core/state/nodes/AgentInformation.h"
#include "core/state/nodes/DeviceInformation.h"
#include "minifi-cpp/agent/agent_version.h"
#include "range/v3/algorithm/contains.hpp"
#include "range/v3/algorithm/find_if.hpp"
#include "unit/Catch.h"
#include "unit/TestBase.h"

TEST_CASE("Test Required", "[required]") {
  const auto standard_processor_bundle_id = minifi::BundleIdentifier{.name = "minifi-standard-processors", .version = minifi::AgentBuild::VERSION};
  const auto standard_processors_components = minifi::ClassDescriptionRegistry::getClassDescriptions().at(standard_processor_bundle_id);
  auto serialized = minifi::state::response::serializeComponentManifest(standard_processors_components);
  REQUIRE_FALSE(serialized.empty());
  const auto &resp = serialized[0];
  REQUIRE_FALSE(resp.children.empty());
  size_t processorIndex = resp.children.size();
  for (size_t i = 0; i < resp.children.size(); ++i) {
    if (resp.children[i].name == "processors") {
      processorIndex = i;
      break;
    }
  }
  REQUIRE(processorIndex < resp.children.size());

  const auto& processors = resp.children[processorIndex];
  const auto get_file_it = ranges::find_if(processors.children, [](const auto& child) {
    return child.name == "org.apache.nifi.minifi.processors.GetFile";
  });
  REQUIRE(get_file_it != processors.children.end());

  REQUIRE_FALSE(get_file_it->children.empty());
  const auto& get_file_property_descriptors = get_file_it->children[0];
  const auto batch_size_property_it = ranges::find_if(get_file_property_descriptors.children, [](const auto& property) {
    return property.name == "Batch Size";
  });
  REQUIRE(batch_size_property_it != get_file_property_descriptors.children.end());

  const auto& batch_size_property = *batch_size_property_it;
  REQUIRE(batch_size_property.children.size() >= 4);
  const auto &batch_size_property_required = batch_size_property.children[3];
  REQUIRE("required" == batch_size_property_required.name);
  REQUIRE(false == std::dynamic_pointer_cast<minifi::state::response::BoolValue>(batch_size_property_required.value.getValue())->getValue());
}

TEST_CASE("Test Valid Regex", "[validRegex]") {
  const auto standard_processor_bundle_id = minifi::BundleIdentifier{.name = "minifi-standard-processors", .version = minifi::AgentBuild::VERSION};
  const auto standard_processors_components = minifi::ClassDescriptionRegistry::getClassDescriptions().at(standard_processor_bundle_id);
  auto serialized = minifi::state::response::serializeComponentManifest(standard_processors_components);
  REQUIRE_FALSE(serialized.empty());
  const auto &resp = serialized[0];
  REQUIRE_FALSE(resp.children.empty());
  const auto &processors = resp.children[0];
  REQUIRE_FALSE(processors.children.empty());
  const auto &processor_with_properties = "org.apache.nifi.minifi.processors.UpdateAttribute" != processors.children[0].name ? processors.children[0] : processors.children[1];
  REQUIRE_FALSE(processor_with_properties.children.empty());
  const auto &prop_descriptors = processor_with_properties.children[0];
  REQUIRE_FALSE(prop_descriptors.children.empty());
  const auto &prop_0 = prop_descriptors.children[0];
  REQUIRE(prop_0.children.size() >= 6);
  CHECK("required" == prop_0.children[3].name);
  CHECK("sensitive" == prop_0.children[4].name);
  CHECK("expressionLanguageScope" == prop_0.children[5].name);
}

TEST_CASE("Test Relationships", "[rel1]") {
  const auto standard_processor_bundle_id = minifi::BundleIdentifier{.name = "minifi-standard-processors", .version = minifi::AgentBuild::VERSION};
  const auto standard_processors_components = minifi::ClassDescriptionRegistry::getClassDescriptions().at(standard_processor_bundle_id);
  auto serialized = minifi::state::response::serializeComponentManifest(standard_processors_components);
  REQUIRE_FALSE(serialized.empty());
  const auto &resp = serialized[0];
  REQUIRE_FALSE(resp.children.empty());
  const auto &processors = resp.children[0];
  REQUIRE_FALSE(processors.children.empty());
  minifi::state::response::SerializedResponseNode proc_0;
  for (const auto& node : processors.children) {
    if ("org.apache.nifi.minifi.processors.PutFile" == node.name) {
      proc_0 = node;
      break;
    }
  }
#ifndef WIN32
  const auto& inputRequirement = proc_0.children[1];
  REQUIRE(inputRequirement.name == "inputRequirement");
  REQUIRE(inputRequirement.value.to_string() == "INPUT_REQUIRED");

  const auto& isSingleThreaded = proc_0.children[2];
  REQUIRE(isSingleThreaded.name == "isSingleThreaded");
  REQUIRE(isSingleThreaded.value.getValue()->getTypeIndex() == org::apache::nifi::minifi::state::response::Value::BOOL_TYPE);
  REQUIRE(isSingleThreaded.value.to_string() == "false");

  REQUIRE_FALSE(proc_0.children.empty());
  const auto& relationships = proc_0.children[3];
  REQUIRE("supportedRelationships" == relationships.name);
  // this is because they are now nested
  REQUIRE("supportedRelationships" == relationships.children[0].name);
  REQUIRE("name" == relationships.children[0].children[0].name);
  REQUIRE("success" == relationships.children[0].children[0].value.to_string());
  REQUIRE("description" == relationships.children[0].children[1].name);

  REQUIRE("failure" == relationships.children[1].children[0].value.to_string());
  REQUIRE("description" == relationships.children[1].children[1].name);
#endif
}

TEST_CASE("Test Dependent", "[dependent]") {
  const auto standard_processor_bundle_id = minifi::BundleIdentifier{.name = "minifi-standard-processors", .version = minifi::AgentBuild::VERSION};
  const auto standard_processors_components = minifi::ClassDescriptionRegistry::getClassDescriptions().at(standard_processor_bundle_id);
  auto serialized = minifi::state::response::serializeComponentManifest(standard_processors_components);
  REQUIRE_FALSE(serialized.empty());
  const auto &resp = serialized[0];
  REQUIRE_FALSE(resp.children.empty());
  const auto &processors = resp.children[0];
  REQUIRE_FALSE(processors.children.empty());
  minifi::state::response::SerializedResponseNode proc_0;
  for (const auto &node : processors.children) {
    if ("org.apache.nifi.minifi.processors.PutFile" == node.name) {
      proc_0 = node;
    }
  }
#ifndef WIN32
  REQUIRE_FALSE(proc_0.children.empty());
  const auto &prop_descriptors = proc_0.children[0];
  REQUIRE(prop_descriptors.children.size() >= 3);
  const auto &prop_0 = prop_descriptors.children[1];
  REQUIRE(prop_0.children.size() >= 6);
  CHECK("required" == prop_0.children[3].name);
  CHECK("sensitive" == prop_0.children[4].name);
  CHECK("expressionLanguageScope" == prop_0.children[5].name);
  CHECK("Directory" == prop_descriptors.children[2].name);
#endif
}

TEST_CASE("Test Scheduling Defaults", "[schedDef]") {
  minifi::state::response::AgentManifest manifest("minifi-system");
  auto serialized = manifest.serialize();
  REQUIRE_FALSE(serialized.empty());
  minifi::state::response::SerializedResponseNode proc_0;
  for (const auto &node : serialized) {
    if ("schedulingDefaults" == node.name) {
      proc_0 = node;
    }
  }
  REQUIRE(proc_0.children.size() == 6);
  for (const auto &child : proc_0.children) {
    if ("defaultMaxConcurrentTasks" == child.name) {
      REQUIRE("1" == child.value.to_string());
    } else if ("defaultRunDurationNanos" == child.name) {
      REQUIRE("0" == child.value.to_string());
    } else if ("defaultSchedulingPeriodMillis" == child.name) {
      REQUIRE("1000" == child.value.to_string());
    } else if ("defaultSchedulingStrategy" == child.name) {
      REQUIRE("TIMER_DRIVEN" == child.value.to_string());
    } else if ("penalizationPeriodMillis" == child.name) {
      REQUIRE("30000" == child.value.to_string());
    } else if ("yieldDurationMillis" == child.name) {
      REQUIRE("1000" == child.value.to_string());
    } else {
      FAIL("UNKNOWN NODE");
    }
  }
}

TEST_CASE("Test operatingSystem Defaults", "[opsys]") {
  minifi::state::response::DeviceInfoNode manifest("minifi-system");
  auto serialized = manifest.serialize();
  REQUIRE_FALSE(serialized.empty());
  minifi::state::response::SerializedResponseNode proc_0;
  for (const auto &node : serialized) {
    if ("systemInfo" == node.name) {
      for (const auto &sinfo : node.children) {
        if ("operatingSystem" == sinfo.name) {
          proc_0 = sinfo;
          break;
        }
      }
    }
  }
  REQUIRE(!proc_0.value.empty());
  std::set<std::string> expected({"Linux", "Windows", "Mac OSX", "Unix"});
  REQUIRE(expected.find(proc_0.value.to_string()) != std::end(expected));
}

namespace {
std::vector<std::string> listExtensionsInManifest(minifi::state::response::AgentManifest& manifest) {
  std::vector<std::string> extensions;
  const auto serialized = manifest.serialize();
  for (const auto& node : serialized) {
    if ("bundles" != node.name) { continue; }
    for (const auto& subnode : node.children) {
      if ("artifact" != subnode.name) { continue; }
      extensions.push_back(subnode.value.to_string());
    }
  }
  return extensions;
}
}  // namespace

TEST_CASE("Compiled but not loaded extensions are not included in the manifest") {
  minifi::state::response::AgentManifest manifest("minifi-system");
  const auto extensions = listExtensionsInManifest(manifest);
  CHECK(ranges::contains(extensions, "minifi-standard-processors"));
  CHECK_FALSE(ranges::contains(extensions, "minifi-test-processors"));
}

enum ComponentType {
  kProcessor,
  kControllerService,
};

struct AllowedType {
  std::string type;
  std::string group;
  std::string artifact;

  auto operator<=>(const AllowedType&) const = default;
};

using minifi::state::response::SerializedResponseNode;

const SerializedResponseNode* getBundle(const std::vector<SerializedResponseNode>& manifest, const std::string_view bundle_artifact_name) {
  const auto bundle_it = ranges::find_if(manifest, [bundle_artifact_name](const auto& node) {
    return node.name == "bundles" && std::end(node.children) != ranges::find_if(node.children, [bundle_artifact_name](const auto& child) {
      return child.name == "artifact" && child.value.to_string() == bundle_artifact_name;
    });
  });
  if (bundle_it == std::end(manifest)) {
    return nullptr;
  }
  return &(*bundle_it);
}


const SerializedResponseNode* getComponentFromBundle(const auto& bundle, const std::string_view name, const ComponentType type) {
  const auto component_manifest = ranges::find_if(bundle.children, [](const auto& bundle_child) { return bundle_child.name == "componentManifest"; });
  if (component_manifest == std::end(bundle.children)) {
    return nullptr;
  }
  if (type == ComponentType::kProcessor) {
    const auto processors = ranges::find_if(component_manifest->children, [](const auto& c) { return c.name == "processors"; });
    if (processors != std::end(component_manifest->children)) {
      const auto proc_it = ranges::find_if(processors->children, [name](const auto& c) { return c.name == name; });
      if (proc_it != std::end(processors->children)) {
        return &(*proc_it);
      }
    }
  } else if (type == ComponentType::kControllerService) {
    const auto controller_services = ranges::find_if(component_manifest->children, [](const auto& c) { return c.name == "controllerServices"; });
    if (controller_services != std::end(component_manifest->children)) {
      const auto controller_service_it = ranges::find_if(controller_services->children, [name](const auto& c) { return c.name == name; });
      if (controller_service_it != std::end(controller_services->children)) {
        return &(*controller_service_it);
      }
    }
  }
  return nullptr;
}

std::optional<AllowedType> getProcessorPropertyAllowedType(const SerializedResponseNode& processor_node, const std::string_view property) {
  const auto property_descriptors = ranges::find_if(processor_node.children, [](const auto& c) { return c.name == "propertyDescriptors"; });
  if (property_descriptors == std::end(processor_node.children)) {
    return std::nullopt;
  }
  const auto property_descriptor = ranges::find_if(property_descriptors->children, [property](const auto& c) { return c.name == property; });
  if (property_descriptor == std::end(property_descriptors->children)) {
    return std::nullopt;
  }
  const auto type_provided_by_value = ranges::find_if(property_descriptor->children, [](const auto& c) { return c.name == "typeProvidedByValue"; });
  if (type_provided_by_value == std::end(property_descriptor->children)) {
    return std::nullopt;
  }
  const auto artifact_node = ranges::find_if(type_provided_by_value->children, [](const auto& c) { return c.name == "artifact"; });
  const auto group_node = ranges::find_if(type_provided_by_value->children, [](const auto& c) { return c.name == "group"; });
  const auto type_node = ranges::find_if(type_provided_by_value->children, [](const auto& c) { return c.name == "type"; });
  if (artifact_node == std::end(type_provided_by_value->children) || group_node == std::end(type_provided_by_value->children) || type_node == std::end(type_provided_by_value->children)) {
    return std::nullopt;
  }
  return AllowedType{
    .type = type_node->value.to_string(),
    .group = group_node->value.to_string(),
    .artifact = artifact_node->value.to_string()};
}

std::vector<AllowedType> getControllerServiceProvidedApiImplementations(const SerializedResponseNode& controller_service_node) {
  std::vector<AllowedType> allowed_types;
  const auto provided_api_implementations = ranges::find_if(controller_service_node.children, [](const auto& c) { return c.name == "providedApiImplementations"; });
  if (provided_api_implementations == std::end(controller_service_node.children)) {
    return allowed_types;
  }
  for (const auto& provided_api_implementation : provided_api_implementations->children) {
    const auto artifact_node = ranges::find_if(provided_api_implementation.children, [](const auto& c) { return c.name == "artifact"; });
    const auto group_node = ranges::find_if(provided_api_implementation.children, [](const auto& c) { return c.name == "group"; });
    const auto type_node = ranges::find_if(provided_api_implementation.children, [](const auto& c) { return c.name == "type"; });
    if (artifact_node == std::end(provided_api_implementation.children)
      || group_node == std::end(provided_api_implementation.children)
      || type_node == std::end(provided_api_implementation.children)) {
      continue;
    }
    allowed_types.push_back({
      .type = type_node->value.to_string(),
      .group = group_node->value.to_string(),
      .artifact = artifact_node->value.to_string()});
  }
  return allowed_types;
}

TEST_CASE("Test providedApiImplementations") {
  minifi::state::response::AgentManifest manifest("minifi-system");
  const auto manifest_serialized = manifest.serialize();

  const auto minifi_system_bundle = getBundle(manifest_serialized, "minifi-system");
  const auto minifi_standard_processors_bundle = getBundle(manifest_serialized, "minifi-standard-processors");

  REQUIRE(minifi_system_bundle);
  REQUIRE(minifi_standard_processors_bundle);

  {
    const auto ssl_context_service = getComponentFromBundle(*minifi_system_bundle, "org.apache.nifi.minifi.controllers.SSLContextService", ComponentType::kControllerService);
    const auto listen_tcp = getComponentFromBundle(*minifi_standard_processors_bundle, "org.apache.nifi.minifi.processors.ListenTCP", ComponentType::kProcessor);

    REQUIRE(ssl_context_service);
    REQUIRE(listen_tcp);

    const auto listen_tcp_ssl_context_service_allowed_type = getProcessorPropertyAllowedType(*listen_tcp, "SSL Context Service");
    const auto ssl_context_service_provided_api_imps = getControllerServiceProvidedApiImplementations(*ssl_context_service);

    REQUIRE(listen_tcp_ssl_context_service_allowed_type);
    REQUIRE(ssl_context_service_provided_api_imps.size() == 1);
    CHECK(*listen_tcp_ssl_context_service_allowed_type == ssl_context_service_provided_api_imps[0]);
  }

  {
    const auto json_tree_reader = getComponentFromBundle(*minifi_standard_processors_bundle, "org.apache.nifi.minifi.standard.JsonTreeReader", ComponentType::kControllerService);
    const auto json_record_set_writer = getComponentFromBundle(*minifi_standard_processors_bundle, "org.apache.nifi.minifi.standard.JsonRecordSetWriter", ComponentType::kControllerService);
    const auto split_record = getComponentFromBundle(*minifi_standard_processors_bundle, "org.apache.nifi.minifi.processors.SplitRecord", ComponentType::kProcessor);

    REQUIRE(json_tree_reader);
    REQUIRE(json_record_set_writer);
    REQUIRE(split_record);

    const auto split_record_record_reader_allowed_type = getProcessorPropertyAllowedType(*split_record, "Record Reader");
    const auto split_record_record_writer_allowed_type = getProcessorPropertyAllowedType(*split_record, "Record Writer");

    const auto json_tree_reader_api_imps = getControllerServiceProvidedApiImplementations(*json_tree_reader);
    const auto json_record_set_writer_api_imps = getControllerServiceProvidedApiImplementations(*json_record_set_writer);

    REQUIRE(split_record_record_reader_allowed_type);
    REQUIRE(split_record_record_writer_allowed_type);

    REQUIRE(json_tree_reader_api_imps.size() == 1);
    REQUIRE(json_record_set_writer_api_imps.size() == 1);

    CHECK(*split_record_record_reader_allowed_type == json_tree_reader_api_imps[0]);
    CHECK(*split_record_record_writer_allowed_type == json_record_set_writer_api_imps[0]);
  }
}
