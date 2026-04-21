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

#include "AgentDocs.h"

#include <algorithm>
#include <iostream>
#include <ranges>
#include <string>
#include <utility>
#include <vector>

#include "TableFormatter.h"
#include "agent/agent_docs.h"
#include "core/Core.h"
#include "core/Relationship.h"
#include "core/state/nodes/AgentInformation.h"
#include "minifi-cpp/agent/agent_version.h"
#include "minifi-cpp/core/PropertyValidator.h"
#include "range/v3/algorithm/lexicographical_compare.hpp"
#include "range/v3/range/conversion.hpp"
#include "range/v3/view/join.hpp"
#include "range/v3/view/transform.hpp"
#include "utils/StringUtils.h"
#include "utils/file/FileUtils.h"

namespace {

namespace minifi = org::apache::nifi::minifi;


void sortClassDescription(minifi::ClassDescription& class_description) {
  std::ranges::sort(class_description.class_properties_, {}, &minifi::core::Property::getName);
  std::ranges::sort(class_description.dynamic_properties_, {}, &minifi::core::DynamicProperty::name);
  std::ranges::sort(class_description.class_relationships_, {}, &minifi::core::Relationship::getName);
  std::ranges::sort(class_description.output_attributes_, {}, &minifi::core::OutputAttribute::name);
  std::ranges::sort(class_description.api_implementations, {}, &minifi::core::ControllerServiceType::type);
}

void sortComponents(minifi::Components& components) {
  auto lower_case_short_name = [](const auto& b) { return minifi::utils::string::toLower(b.short_name_); };
  std::ranges::sort(components.processors, {}, lower_case_short_name);
  std::ranges::sort(components.controller_services, {}, lower_case_short_name);
  std::ranges::sort(components.parameter_providers, {}, lower_case_short_name);
  std::ranges::sort(components.other_components, {}, lower_case_short_name);

  for (auto& processors : components.processors) {
    sortClassDescription(processors);
  }
  for (auto& cs : components.controller_services) {
    sortClassDescription(cs);
  }
  for (auto& pp : components.parameter_providers) {
    sortClassDescription(pp);
  }
  for (auto& oc : components.other_components) {
    sortClassDescription(oc);
  }
}

std::string formatName(std::string_view name_view, bool is_required) {
  std::string name{name_view};
  if (is_required) { return "**" + name + "**"; }
  return name;
}

std::string formatAllowedValues(const minifi::core::Property& property) {
  if (property.getValidator().getEquivalentNifiStandardValidatorName() ==
      minifi::core::StandardPropertyValidators::BOOLEAN_VALIDATOR.getEquivalentNifiStandardValidatorName()) {
    return "true<br/>false";
  }
  const auto allowed_values = property.getAllowedValues();
  return allowed_values | ranges::views::join(std::string_view{"<br/>"}) | ranges::to<std::string>();
}

std::string formatDescription(std::string_view description_view, bool is_sensitive = false, bool supports_expression_language = false) {
  std::string description{description_view};
  minifi::utils::string::replaceAll(description, "\n", "<br/>");
  if (is_sensitive) { description += "<br/>**Sensitive Property: true**"; }
  if (supports_expression_language) { description += "<br/>**Supports Expression Language: true**"; }
  return description;
}

std::string formatDescription(const minifi::core::Property& property) {
  return formatDescription(property.getDescription(), property.isSensitive(), property.supportsExpressionLanguage());
}

std::string formatDescription(const minifi::core::DynamicProperty& dynamic_property) {
  return formatDescription(dynamic_property.description, false, dynamic_property.supports_expression_language);
}

std::string formatListOfRelationships(std::span<const minifi::core::Relationship> relationships) {
  return minifi::utils::string::join(", ", relationships, [](const auto& relationship) { return relationship.getName(); });
}

inline constexpr std::string_view APACHE_LICENSE = R"license(<!--
Licensed to the Apache Software Foundation (ASF) under one or more
contributor license agreements.  See the NOTICE file distributed with
this work for additional information regarding copyright ownership.
The ASF licenses this file to You under the Apache License, Version 2.0
(the "License"); you may not use this file except in compliance with
the License.  You may obtain a copy of the License at
    http://www.apache.org/licenses/LICENSE-2.0
Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
-->)license";

void writeHeader(std::ostream& docs, const std::vector<std::pair<std::string, minifi::ClassDescription>>& class_descriptions) {
  docs << APACHE_LICENSE;

  docs << "\n\n## Table of Contents\n\n";
  for (const auto& [name, documentation] : class_descriptions) {
    docs << "- [" << name << "](#" << name << ")\n";
  }
}

void writeName(std::ostream& docs, std::string_view name) {
  docs << "\n\n## " << name;
}

void writeDescription(std::ostream& docs, const minifi::ClassDescription& documentation) {
  docs << "\n\n### Description\n\n";
  docs << documentation.description_;
}

void writeProperties(std::ostream& docs, const minifi::ClassDescription& documentation) {
  docs << "\n\n### Properties";
  docs << "\n\nIn the list below, the names of required properties appear in bold. Any other properties (not in bold) are considered optional. "
       << "The table also indicates any default values, and whether a property supports the NiFi Expression Language.";
  minifi::docs::Table properties{{"Name", "Default Value", "Allowable Values", "Description"}};
  for (const auto& property : documentation.class_properties_) {
    properties.addRow({formatName(property.getName(), property.getRequired()),
        property.getDefaultValue().value_or(""),
        formatAllowedValues(property),
        formatDescription(property)});
  }
  docs << "\n\n" << properties.toString();
}

void writeDynamicProperties(std::ostream& docs, const minifi::ClassDescription& documentation) {
  if (documentation.dynamic_properties_.empty()) { return; }

  docs << "\n### Dynamic Properties\n\n";
  minifi::docs::Table dynamic_properties{{"Name", "Value", "Description"}};
  for (const auto& dynamic_property : documentation.dynamic_properties_) {
    dynamic_properties.addRow({formatName(dynamic_property.name, false), std::string(dynamic_property.value), formatDescription(dynamic_property)});
  }
  docs << dynamic_properties.toString();
}

void writeRelationships(std::ostream& docs, const minifi::ClassDescription& documentation) {
  docs << "\n### Relationships\n\n";
  minifi::docs::Table relationships{{"Name", "Description"}};
  for (const auto& rel : documentation.class_relationships_) {
    relationships.addRow({rel.getName(), formatDescription(rel.getDescription())});
  }
  docs << relationships.toString();
}

void writeOutputAttributes(std::ostream& docs, const minifi::ClassDescription& documentation) {
  if (documentation.output_attributes_.empty()) { return; }

  docs << "\n### Output Attributes";
  minifi::docs::Table output_attributes{{"Attribute", "Relationship", "Description"}};
  for (const auto& output_attribute : documentation.output_attributes_) {
    output_attributes.addRow({std::string(output_attribute.name),
        formatListOfRelationships(output_attribute.relationships),
        formatDescription(output_attribute.description)});
  }
  docs << "\n\n" << output_attributes.toString();
}

std::string extractClassName(const std::string& full_class_name) {
  return minifi::utils::string::split(full_class_name, ".").back();
}

void writeProcessor(std::ostream& os, const std::string_view name, const minifi::ClassDescription& documentation) {
  writeName(os, name);
  writeDescription(os, documentation);
  writeProperties(os, documentation);
  writeDynamicProperties(os, documentation);
  writeRelationships(os, documentation);
  writeOutputAttributes(os, documentation);
}

void writeControllerService(std::ostream& os, const std::string_view name, const minifi::ClassDescription& documentation) {
  writeName(os, name);
  writeDescription(os, documentation);
  writeProperties(os, documentation);
}

void writeParameterProvider(std::ostream& os, const std::string_view name, const minifi::ClassDescription& documentation) {
  writeName(os, name);
  writeDescription(os, documentation);
  writeProperties(os, documentation);
}

class MonolithDocumentation {
 public:
  explicit MonolithDocumentation() {
    for (const auto& [bundle_id, components] : minifi::ClassDescriptionRegistry::getClassDescriptions()) {
      addComponents(components);
    }
    sort();
  }

  void write(const std::filesystem::path& docs_dir) {
    std::ofstream controllers_md(docs_dir / "CONTROLLERS.md");
    std::ofstream processors_md(docs_dir / "PROCESSORS.md");
    std::ofstream parameter_providers_md(docs_dir / "PARAMETER_PROVIDERS.md");
    gsl_Assert(controllers_md && processors_md && parameter_providers_md);

    writeControllers(controllers_md);
    writeProcessors(processors_md);
    writeParameterProviders(parameter_providers_md);
  }

 private:
  void addComponents(const minifi::Components& components) {
    for (const auto& controller_service_description : components.controller_services) {
      controller_services.emplace_back(extractClassName(controller_service_description.full_name_), controller_service_description);
    }
    for (const auto& processor_description : components.processors) {
      processors.emplace_back(extractClassName(processor_description.full_name_), processor_description);
    }
    for (const auto& parameter_provider_description : components.parameter_providers) {
      parameter_providers.emplace_back(extractClassName(parameter_provider_description.full_name_), parameter_provider_description);
    }
  }

  void sort() {
    auto lower_case_first = [](const auto& kv) { return minifi::utils::string::toLower(kv.first); };
    std::ranges::sort(controller_services, std::less(), lower_case_first);
    std::ranges::sort(processors, std::less(), lower_case_first);
    std::ranges::sort(parameter_providers, std::less(), lower_case_first);
  }

  void writeControllers(std::ostream& os) {
    writeHeader(os, controller_services);
    for (const auto& [name, documentation] : controller_services) {
      writeControllerService(os, name, documentation);
    }
  }

  void writeProcessors(std::ostream& os) {
    writeHeader(os, processors);
    for (const auto& [name, documentation] : processors) {
      writeProcessor(os, name, documentation);
    }
  }

  void writeParameterProviders(std::ostream& os) {
    writeHeader(os, parameter_providers);
    for (const auto& [name, documentation] : parameter_providers) {
      writeParameterProvider(os, name, documentation);
    }
  }

  std::vector<std::pair<std::string, minifi::ClassDescription>> controller_services;
  std::vector<std::pair<std::string, minifi::ClassDescription>> processors;
  std::vector<std::pair<std::string, minifi::ClassDescription>> parameter_providers;
};

class ModularDocumentation {
 public:
  static void write(const std::filesystem::path& docs_dir) {
    for (auto [bundle_id, components] : minifi::ClassDescriptionRegistry::getClassDescriptions()) {
      if (components.empty()) { continue; }
      sortComponents(components);
      writeModule(docs_dir, bundle_id.name, components);
    }
  }

 private:
  static void writeComponentParts(std::ostream& os, const std::vector<minifi::ClassDescription>& class_descriptions, const std::string_view h3) {
    if (!class_descriptions.empty()) {
      os << fmt::format("### {}\n\n", h3);
      for (const auto& class_description : class_descriptions) {
        const auto name = extractClassName(class_description.full_name_);
        os << "- [" << name << "](#" << name << ")\n";
      }
    }
  }

  static void writeToC(std::ostream& os, const minifi::Components& components) {
    os << "\n\n## Table of Contents\n\n";
    writeComponentParts(os, components.processors, "Processors");
    writeComponentParts(os, components.controller_services, "Controller Services");
    writeComponentParts(os, components.parameter_providers, "Parameter Providers");
  }

  static void writeModule(const std::filesystem::path& docs_dir, const std::string_view module_name, const minifi::Components& components) {
    const auto dir_creation_result = minifi::utils::file::create_dir(docs_dir / "modules");
    gsl_Assert(dir_creation_result == 0);
    std::ofstream os(docs_dir / "modules" / (std::string(module_name) + ".md"));
    gsl_Assert(os);
    os << APACHE_LICENSE;

    writeToC(os, components);

    for (const auto& processor : components.processors) {
      writeProcessor(os, extractClassName(processor.full_name_), processor);
    }

    for (const auto& controller_service : components.controller_services) {
      writeControllerService(os, extractClassName(controller_service.full_name_), controller_service);
    }

    for (const auto& parameter_provider_description : components.parameter_providers) {
      writeParameterProvider(os, extractClassName(parameter_provider_description.full_name_), parameter_provider_description);
    }
  }
};
}  // namespace

namespace org::apache::nifi::minifi::docs {

void AgentDocs::generate(const std::filesystem::path& docs_dir) {
  MonolithDocumentation monolith_docs;
  monolith_docs.write(docs_dir);

  ModularDocumentation::write(docs_dir);
}

void AgentDocs::generateManifest() {
  auto all_components = Components{};
  for (const auto& components : minifi::ClassDescriptionRegistry::getClassDescriptions() | std::views::values) {
    std::ranges::copy(components.processors, std::back_inserter(all_components.processors));
    std::ranges::copy(components.controller_services, std::back_inserter(all_components.controller_services));
    std::ranges::copy(components.parameter_providers, std::back_inserter(all_components.parameter_providers));
    std::ranges::copy(components.other_components, std::back_inserter(all_components.other_components));
  }

  sortComponents(all_components);
  auto serialized = minifi::state::response::serializeComponentManifest(all_components);
  for (const auto& ser : serialized) {
    std::cout << ser.to_pretty_string() << std::endl;
  }}

}  // namespace org::apache::nifi::minifi::docs
