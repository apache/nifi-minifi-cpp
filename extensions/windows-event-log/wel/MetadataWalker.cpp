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

#include <windows.h>
#include <strsafe.h>

#include <map>
#include <functional>
#include <codecvt>
#include <regex>
#include <string>
#include <utility>
#include <vector>

#include "MetadataWalker.h"
#include "XMLString.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace wel {

bool MetadataWalker::for_each(pugi::xml_node &node) {
  // don't shortcut resolution here so that we can log attributes.
  const std::string node_name = node.name();
  if (node_name == "Data") {
    for (pugi::xml_attribute attr : node.attributes())  {
      const auto idUpdate = [&](const std::string &input) {
        if (resolve_) {
          const auto resolved = utils::OsUtils::userIdToUsername(input);
          replaced_identifiers_[input] = resolved;
          return resolved;
        }

        replaced_identifiers_[input] = input;
        return input;
      };

      if (regex_ && utils::regexMatch(attr.name(), *regex_)) {
        updateText(node, attr.name(), idUpdate);
      }

      if (regex_ && utils::regexMatch(attr.value(), *regex_)) {
        updateText(node, attr.value(), idUpdate);
      }
    }

    if (resolve_) {
      std::string nodeText = node.text().get();
      std::vector<std::string> ids = getIdentifiers(nodeText);
      for (const auto &id : ids) {
        auto  resolved = utils::OsUtils::userIdToUsername(id);
        std::string replacement = "%{" + id + "}";
        replaced_identifiers_[id] = resolved;
        replaced_identifiers_[replacement] = resolved;
        nodeText = utils::StringUtils::replaceAll(nodeText, replacement, resolved);
      }
      node.text().set(nodeText.c_str());
    }
  } else if (node_name == "TimeCreated") {
    metadata_["TimeCreated"] = node.attribute("SystemTime").value();
  } else if (node_name == "EventRecordID") {
    metadata_["EventRecordID"] = node.text().get();
  } else if (node_name == "Provider") {
    metadata_["Provider"] = node.attribute("Name").value();
  } else if (node_name == "EventID") {
    metadata_["EventID"] = node.text().get();
  } else {
    static std::map<std::string, EVT_FORMAT_MESSAGE_FLAGS> formatFlagMap = {
        {"Channel", EvtFormatMessageChannel}, {"Keywords", EvtFormatMessageKeyword}, {"Level", EvtFormatMessageLevel},
        {"Opcode", EvtFormatMessageOpcode}, {"Task", EvtFormatMessageTask}
    };
    auto it = formatFlagMap.find(node_name);
    if (it != formatFlagMap.end()) {
      std::function<std::string(const std::string &)> updateFunc = [&](const std::string &input) -> std::string {
        if (resolve_) {
          auto resolved = windows_event_log_metadata_.getEventData(it->second);
          if (!resolved.empty()) {
            return resolved;
          }
        }
        return input;
      };
      updateText(node, node.name(), std::move(updateFunc));
    } else {
      // no conversion is required here, so let the node fall through
    }
  }

  return true;
}

std::vector<std::string> MetadataWalker::getIdentifiers(const std::string &text) const {
  auto pos = text.find("%{");
  std::vector<std::string> found_strings;
  while (pos != std::string::npos) {
    auto next_pos = text.find("}", pos);
    if (next_pos != std::string::npos) {
      auto potential_identifier = text.substr(pos + 2, next_pos - (pos + 2));
      std::smatch match;
      if (potential_identifier.find("S-") != std::string::npos) {
        found_strings.push_back(potential_identifier);
      }
    }
    pos = text.find("%{", pos + 2);
  }
  return found_strings;
}

std::string MetadataWalker::getMetadata(METADATA metadata) const {
    switch (metadata) {
      case LOG_NAME:
        return log_name_;
      case SOURCE:
        return getString(metadata_, "Provider");
      case TIME_CREATED:
        return windows_event_log_metadata_.getEventTimestamp();
      case EVENTID:
        return getString(metadata_, "EventID");
      case EVENT_RECORDID:
        return getString(metadata_, "EventRecordID");
      case OPCODE:
        return getString(metadata_, "Opcode");
      case TASK_CATEGORY:
        return getString(metadata_, "Task");
      case LEVEL:
        return getString(metadata_, "Level");
      case KEYWORDS:
        return getString(metadata_, "Keywords");
      case EVENT_TYPE:
        return std::to_string(windows_event_log_metadata_.getEventTypeIndex());
      case COMPUTER:
        return windows_event_log_metadata_.getComputerName();
    }
    return "N/A";
}

std::map<std::string, std::string> MetadataWalker::getFieldValues() const {
  return fields_values_;
}

std::map<std::string, std::string> MetadataWalker::getIdentifiers() const {
  return replaced_identifiers_;
}

std::string MetadataWalker::updateXmlMetadata(const std::string &xml, EVT_HANDLE metadata_ptr, EVT_HANDLE event_ptr, bool update_xml, bool resolve, const std::string &regex) {
  WindowsEventLogMetadataImpl metadata{metadata_ptr, event_ptr};
  MetadataWalker walker(metadata, "", update_xml, resolve, regex);

  pugi::xml_document doc;
  pugi::xml_parse_result result = doc.load_string(xml.c_str());

  if (result) {
    doc.traverse(walker);
    wel::XmlString writer;
    doc.print(writer, "", pugi::format_raw);  // no indentation or formatting
    return writer.xml_;
  } else {
    throw std::runtime_error("Could not parse XML document");
  }
}

std::string MetadataWalker::to_string(const wchar_t* pChar) {
  return std::wstring_convert<std::codecvt_utf8<wchar_t>>().to_bytes(pChar);
}

void MetadataWalker::updateText(pugi::xml_node &node, const std::string &field_name, std::function<std::string(const std::string &)> &&fn) {
  std::string previous_value = node.text().get();
  auto new_field_value = fn(previous_value);
  if (new_field_value != previous_value) {
    metadata_[field_name] = new_field_value;
    if (update_xml_) {
      node.text().set(new_field_value.c_str());
    } else {
      fields_values_[field_name] = new_field_value;
    }
  }
}

} /* namespace wel */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
