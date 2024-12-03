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

#include <iostream>
#include <string>
#include <utility>
#include <vector>

#include "range/v3/algorithm.hpp"
#include "range/v3/action/transform.hpp"
#include "range/v3/algorithm/lexicographical_compare.hpp"
#include "range/v3/range/conversion.hpp"
#include "range/v3/view/transform.hpp"
#include "range/v3/view/join.hpp"

#include "agent/agent_docs.h"
#include "agent/agent_version.h"
#include "core/Core.h"
#include "core/PropertyValue.h"
#include "core/PropertyType.h"
#include "core/Relationship.h"
#include "TableFormatter.h"
#include "utils/file/FileUtils.h"
#include "utils/StringUtils.h"

namespace {

namespace minifi = org::apache::nifi::minifi;

std::string formatName(std::string_view name_view, bool is_required) {
  std::string name{name_view};
  if (is_required) {
    return "**" + name + "**";
  } else {
    return name;
  }
}

std::string formatAllowedValues(const minifi::core::Property& property) {
  if (property.getValidator().getValidatorName() == minifi::core::StandardPropertyTypes::BOOLEAN_TYPE.getValidatorName()) {
    return "true<br/>false";
  } else {
    const auto allowed_values = property.getAllowedValues();
    return allowed_values
        | ranges::views::transform([](const auto &value) { return value.to_string(); })
        | ranges::views::join(std::string_view{"<br/>"})
        | ranges::to<std::string>();
  }
}

std::string formatDescription(std::string_view description_view, bool is_sensitive = false, bool supports_expression_language = false) {
  std::string description{description_view};
  minifi::utils::string::replaceAll(description, "\n", "<br/>");
  if (is_sensitive) {
    description += "<br/>**Sensitive Property: true**";
  }
  if (supports_expression_language) {
    description += "<br/>**Supports Expression Language: true**";
  }
  return description;
}

std::string formatDescription(const minifi::core::Property& property) {
  return formatDescription(property.getDescription(), property.isSensitive(), property.supportsExpressionLanguage());
}

std::string formatDescription(const minifi::core::DynamicProperty& dynamic_property) {
  return formatDescription(dynamic_property.description, false, dynamic_property.supports_expression_language);
}

std::string formatListOfRelationships(std::span<const minifi::core::RelationshipDefinition> relationships) {
  return minifi::utils::string::join(", ", relationships, [](const auto& relationship) { return relationship.name; });
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
  for (const auto &property : documentation.class_properties_) {
    properties.addRow({
        formatName(property.getName(), property.getRequired()),
        property.getDefaultValue().to_string(),
        formatAllowedValues(property),
        formatDescription(property)
    });
  }
  docs << "\n\n" << properties.toString();
}

void writeDynamicProperties(std::ostream& docs, const minifi::ClassDescription& documentation) {
  if (documentation.dynamic_properties_.empty()) { return; }

  docs << "\n### Dynamic Properties\n\n";
  minifi::docs::Table dynamic_properties{{"Name", "Value", "Description"}};
  for (const auto &dynamic_property : documentation.dynamic_properties_) {
    dynamic_properties.addRow({
        formatName(dynamic_property.name, false),
        std::string(dynamic_property.value),
        formatDescription(dynamic_property)
    });
  }
  docs << dynamic_properties.toString();
}

void writeRelationships(std::ostream& docs, const minifi::ClassDescription& documentation) {
  docs << "\n### Relationships\n\n";
  minifi::docs::Table relationships{{"Name", "Description"}};
  for (const auto &rel : documentation.class_relationships_) {
    relationships.addRow({rel.getName(), formatDescription(rel.getDescription())});
  }
  docs << relationships.toString();
}

void writeOutputAttributes(std::ostream& docs, const minifi::ClassDescription& documentation) {
  if (documentation.output_attributes_.empty()) { return; }

  docs << "\n### Output Attributes";
  minifi::docs::Table output_attributes{{"Attribute", "Relationship", "Description"}};
  for (const auto &output_attribute : documentation.output_attributes_) {
    output_attributes.addRow({
        std::string(output_attribute.name),
        formatListOfRelationships(output_attribute.relationships),
        formatDescription(output_attribute.description)});
  }
  docs << "\n\n" << output_attributes.toString();
}

std::string extractClassName(const std::string& full_class_name) {
  return minifi::utils::string::split(full_class_name, ".").back();
}

std::string lowercaseFirst(const std::pair<std::string, minifi::ClassDescription>& key_value) {
  return minifi::utils::string::toLower(key_value.first);
};

}  // namespace

namespace org::apache::nifi::minifi::docs {

void AgentDocs::generate(const std::filesystem::path& docs_dir) {
  std::vector<std::pair<std::string, minifi::ClassDescription>> controller_services;
  std::vector<std::pair<std::string, minifi::ClassDescription>> processors;
  for (const auto &group : minifi::AgentBuild::getExtensions()) {
    struct Components descriptions = build_description_.getClassDescriptions(group);
    for (const auto &controller_service_description : descriptions.controller_services_) {
      controller_services.emplace_back(extractClassName(controller_service_description.full_name_), controller_service_description);
    }
    for (const auto &processor_description : descriptions.processors_) {
      processors.emplace_back(extractClassName(processor_description.full_name_), processor_description);
    }
  }
  ranges::sort(controller_services, std::less(), lowercaseFirst);
  ranges::sort(processors, std::less(), lowercaseFirst);

  std::ofstream controllers_md(docs_dir / "CONTROLLERS.md");
  writeHeader(controllers_md, controller_services);
  for (const auto& [name, documentation] : controller_services) {
    writeName(controllers_md, name);
    writeDescription(controllers_md, documentation);
    writeProperties(controllers_md, documentation);
  }

  std::ofstream processors_md(docs_dir / "PROCESSORS.md");
  writeHeader(processors_md, processors);
  for (const auto& [name, documentation] : processors) {
    writeName(processors_md, name);
    writeDescription(processors_md, documentation);
    writeProperties(processors_md, documentation);
    writeDynamicProperties(processors_md, documentation);
    writeRelationships(processors_md, documentation);
    writeOutputAttributes(processors_md, documentation);
  }
}

}  // namespace org::apache::nifi::minifi::docs
