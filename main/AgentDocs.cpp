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

#include "AgentDocs.h"
#include <vector>
#include <algorithm>
#include <iostream>
#include <iterator>
#include <fstream>
#include <set>
#include <memory>
#include <string>
#include "core/ClassLoader.h"
#include "core/ConfigurableComponent.h"
#include "core/Core.h"
#include "core/Processor.h"
#include "core/Property.h"
#include "core/PropertyValidation.h"
#include "core/Relationship.h"
#include "io/validation.h"
#include "utils/file/FileUtils.h"
#include "agent/build_description.h"
#include "agent/agent_docs.h"
#include "agent/agent_version.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace docs {

std::string AgentDocs::extractClassName(const std::string &processor) const {
  auto positionOfLastDot = processor.find_last_of(".");
  if (positionOfLastDot != std::string::npos) {
    return processor.substr(positionOfLastDot + 1);
  }
  return processor;
}

void AgentDocs::generate(const std::string &docsdir, std::ostream &genStream) {
  std::map<std::string, ClassDescription> processorSet;
  for (const auto &group : minifi::AgentBuild::getExtensions()) {
    struct Components descriptions = BuildDescription::getClassDescriptions(group);
    for (const auto &processorName : descriptions.processors_) {
      processorSet.insert(std::make_pair(extractClassName(processorName.class_name_), processorName));
    }
  }
  for (const auto &processor : processorSet) {
    const std::string &filename = docsdir + utils::file::FileUtils::get_separator() + processor.first;
    std::ofstream outfile(filename);

    std::string description;

    bool foundDescription = minifi::AgentDocs::getDescription(processor.first, description);

    if (!foundDescription) {
      foundDescription = minifi::AgentDocs::getDescription(processor.second.class_name_, description);
    }

    outfile << "## " << processor.first << std::endl << std::endl;
    if (foundDescription) {
      outfile << "### Description " << std::endl << std::endl;
      outfile << description << std::endl;
    }

    outfile << "### Properties " << std::endl << std::endl;
    outfile
        << "In the list below, the names of required properties appear in bold. Any other properties (not in bold) are considered optional. The table also indicates any default values, and whether a property supports the NiFi Expression Language."
        << std::endl<< std::endl;

    outfile << "| Name | Default Value | Allowable Values | Description | " << std::endl << "| - | - | - | - | " << std::endl;
    for (const auto &prop : processor.second.class_properties_) {
      bool supportsEl = prop.second.supportsExpressionLangauge();
      outfile << "|";
      if (prop.second.getRequired()) {
        outfile << "**";
      }
      outfile << prop.first;
      if (prop.second.getRequired()) {
        outfile << "**";
      }
      const auto allowableValues = prop.second.getAllowedValues();
      std::stringstream s;
      std::copy(allowableValues.begin(), allowableValues.end(), std::ostream_iterator<std::string>(s, "<br>"));
      outfile << "|";
      const auto defaultValue = prop.second.getDefaultValue().to_string();
      if (defaultValue.size() == 1 && (int)defaultValue.c_str()[0] == 0x0a) {
        outfile << "\\n";
      }
      else {
        outfile << defaultValue;
      }
      std::string description = prop.second.getDescription();
      outfile << "|" << s.str() << "|" << utils::StringUtils::replaceAll(description, "\n", "<br/>");
      if (supportsEl) {
        outfile << "<br/>**Supports Expression Language: true**";
      }
      outfile << "|" << std::endl;
    }
    outfile << "### Properties " << std::endl << std::endl;
    outfile << "| Name | Description |" << std::endl << "| - | - |" << std::endl;
    for (const auto &rel : processor.second.class_relationships_) {
      std::string description = rel.getDescription();
      outfile << "|" << rel.getName() << "|" << utils::StringUtils::replaceAll(description, "\n", "<br/>") << "|" << std::endl;
    }

    outfile << std::endl;
  }

  std::map<std::string, std::string> fileList;

  auto fileFind = [&fileList](const std::string& base_path, const std::string& file) -> bool {
    if (file.find(".extra") == std::string::npos)
      fileList.insert(std::make_pair(file, base_path + utils::file::FileUtils::get_separator() + file));
    return true;
  };

  // shortened with list_dir
  utils::file::FileUtils::list_dir(docsdir, fileFind, logging::LoggerFactory<AgentDocs>::getLogger());
  genStream << "<!--"
  "Licensed to the Apache Software Foundation (ASF) under one or more"
  "contributor license agreements.  See the NOTICE file distributed with"
  "this work for additional information regarding copyright ownership."
  "The ASF licenses this file to You under the Apache License, Version 2.0"
  "(the \"License\"); you may not use this file except in compliance with"
  "the License.  You may obtain a copy of the License at"
  "    http://www.apache.org/licenses/LICENSE-2.0"
  "Unless required by applicable law or agreed to in writing, software"
  "distributed under the License is distributed on an \"AS IS\" BASIS,"
  "WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied."
  "See the License for the specific language governing permissions and"
  "limitations under the License."
  "-->" << std::endl;
  genStream << "# Processors" << std::endl << std::endl;
  genStream << "## Table of Contents" << std::endl << std::endl;

  for (const auto &file : fileList) {
    std::string lcfile = file.first;
    std::transform(lcfile.begin(), lcfile.end(), lcfile.begin(), ::tolower);
    genStream << "- [" << file.first << "](#" << lcfile << ")" << std::endl;
  }

  for (const auto &file : fileList) {
      std::ifstream filestream(file.second);
      genStream << filestream.rdbuf() << std::endl;
      std::ifstream filestreamExtra(file.second + ".extra");
      if (filestreamExtra.good()) {
        genStream << filestreamExtra.rdbuf()<< std::endl;
      }
  }
}

} /* namespace docs */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
