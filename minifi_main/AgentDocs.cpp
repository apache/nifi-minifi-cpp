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

#include <map>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

#include "range/v3/action/transform.hpp"
#include "range/v3/view/transform.hpp"
#include "range/v3/view/join.hpp"
#include "range/v3/range/conversion.hpp"

#include "agent/agent_docs.h"
#include "agent/agent_version.h"
#include "core/Core.h"
#include "core/PropertyValue.h"
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

std::string formatAllowableValues(const std::vector<minifi::core::PropertyValue>& values) {
  return values
      | ranges::views::transform([](const auto& value) { return value.to_string(); })
      | ranges::views::join(std::string_view{"<br/>"})
      | ranges::to<std::string>();
}

std::string formatDescription(std::string_view description_view, bool supports_expression_language = false) {
  std::string description{description_view};
  org::apache::nifi::minifi::utils::StringUtils::replaceAll(description, "\n", "<br/>");
  return supports_expression_language ? description + "<br/>**Supports Expression Language: true**" : description;
}

std::string formatListOfRelationships(std::span<const minifi::core::RelationshipDefinition> relationships) {
  return minifi::utils::StringUtils::join(", ", relationships, [](const auto& relationship) { return relationship.name; });
}

}  // namespace

namespace org::apache::nifi::minifi::docs {

std::string AgentDocs::extractClassName(const std::string &processor) {
  auto positionOfLastDot = processor.find_last_of('.');
  if (positionOfLastDot != std::string::npos) {
    return processor.substr(positionOfLastDot + 1);
  }
  return processor;
}

void AgentDocs::generate(const std::filesystem::path& docsdir, std::ostream &genStream) {
  std::map<std::string, ClassDescription> processorSet;
  for (const auto &group : minifi::AgentBuild::getExtensions()) {
    struct Components descriptions = build_description_.getClassDescriptions(group);
    for (const auto& processor_description : descriptions.processors_) {
      processorSet.insert(std::make_pair(extractClassName(processor_description.full_name_), processor_description));
    }
  }
  for (const auto &processor : processorSet) {
    const auto& filename = docsdir / processor.first;
    std::ofstream outfile(filename);

    outfile << "## " << processor.first << "\n\n";
    outfile << "### Description\n\n";
    outfile << processor.second.description_ << '\n';

    outfile << "\n### Properties\n\n";
    outfile  << "In the list below, the names of required properties appear in bold. Any other properties (not in bold) are considered optional. "
        << "The table also indicates any default values, and whether a property supports the NiFi Expression Language.\n\n";

    Table properties{{"Name", "Default Value", "Allowable Values", "Description"}};
    for (const auto &prop : processor.second.class_properties_) {
      properties.addRow({
          formatName(prop.getName(), prop.getRequired()),
          prop.getDefaultValue().to_string(),
          formatAllowableValues(prop.getAllowedValues()),
          formatDescription(prop.getDescription(), prop.supportsExpressionLanguage())});
    }
    outfile << properties.toString() << '\n';

    if (!processor.second.dynamic_properties_.empty()) {
      outfile << "### Dynamic Properties\n\n";
      Table dynamic_properties{{"Name", "Value", "Description"}};
      for (const auto& dynamic_property : processor.second.dynamic_properties_) {
        dynamic_properties.addRow({
            formatName(dynamic_property.name, false),
            std::string(dynamic_property.value),
            formatDescription(dynamic_property.description, dynamic_property.supports_expression_language)
        });
      }
      outfile << dynamic_properties.toString() << '\n';
    }

    outfile << "### Relationships\n\n";
    Table relationships{{"Name", "Description"}};
    for (const auto &rel : processor.second.class_relationships_) {
      relationships.addRow({rel.getName(), formatDescription(rel.getDescription())});
    }
    outfile << relationships.toString() << '\n';

    if (!processor.second.output_attributes_.empty()) {
      outfile << "### Output Attributes\n\n";
      Table output_attributes{{"Attribute", "Relationship", "Description"}};
      for (const auto& output_attribute : processor.second.output_attributes_) {
        output_attributes.addRow({
            std::string(output_attribute.name),
            formatListOfRelationships(output_attribute.relationships),
            formatDescription(output_attribute.description)});
      }
      outfile << output_attributes.toString() << '\n';
    }
  }

  std::map<std::string, std::filesystem::path> fileList;
  auto fileFind = [&fileList](const std::filesystem::path& base_path, const std::filesystem::path& file) -> bool {
    if (file.string().find(".extra") == std::string::npos) {
      auto file_name = file.string();
      ranges::actions::transform(file_name, [](auto ch) { return ::tolower(static_cast<unsigned char>(ch)); });
      fileList.emplace(file_name, base_path / file);
    }
    return true;
  };
  utils::file::list_dir(docsdir, fileFind, core::logging::LoggerFactory<AgentDocs>::getLogger());

  genStream << "<!--\n"
      "Licensed to the Apache Software Foundation (ASF) under one or more\n"
      "contributor license agreements.  See the NOTICE file distributed with\n"
      "this work for additional information regarding copyright ownership.\n"
      "The ASF licenses this file to You under the Apache License, Version 2.0\n"
      "(the \"License\"); you may not use this file except in compliance with\n"
      "the License.  You may obtain a copy of the License at\n"
      "    http://www.apache.org/licenses/LICENSE-2.0\n"
      "Unless required by applicable law or agreed to in writing, software\n"
      "distributed under the License is distributed on an \"AS IS\" BASIS,\n"
      "WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.\n"
      "See the License for the specific language governing permissions and\n"
      "limitations under the License.\n"
      "-->\n\n";
  genStream << "## Table of Contents\n\n";

  for (const auto& file : fileList) {
    genStream << "- [" << file.second.filename().string() << "](#" << file.first << ")\n";
  }
  genStream << "\n\n";

  for (const auto& file : fileList) {
      std::ifstream filestream(file.second);
      genStream << filestream.rdbuf() << '\n';
      auto extra_path = file.second;
      extra_path += ".extra";
      std::ifstream filestreamExtra(extra_path);
      if (filestreamExtra.good()) {
        genStream << filestreamExtra.rdbuf() << '\n';
      }
  }
}

}  // namespace org::apache::nifi::minifi::docs
