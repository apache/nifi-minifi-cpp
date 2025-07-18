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

#include <strsafe.h>

#include <map>
#include <functional>
#include <regex>
#include <string>
#include <utility>
#include <vector>

#include "MetadataWalker.h"
#include "XMLString.h"

namespace org::apache::nifi::minifi::wel {

bool MetadataWalker::for_each(pugi::xml_node &node) {
  // don't shortcut resolution here so that we can log attributes.
  const auto idUpdate = [&](const std::string &input) {
    if (resolve_) {
      auto resolved = user_id_to_username_fn_(input);
      replaced_identifiers_[input] = resolved;
      return resolved;
    }

    replaced_identifiers_[input] = input;
    return input;
  };
  for (pugi::xml_attribute attr : node.attributes())  {
    if (sid_matcher_(attr.name())) {
      updateAttributeValue(attr, attr.name(), idUpdate);
    }
  }

  const std::string node_name = node.name();
  if (node_name == "Data") {
    for (pugi::xml_attribute attr : node.attributes())  {
      if (sid_matcher_(attr.name())) {
        updateText(node, attr.name(), idUpdate);
      }

      if (sid_matcher_(attr.value())) {
        updateText(node, attr.value(), idUpdate);
      }
    }

    if (resolve_) {
      std::string nodeText = node.text().get();
      std::vector<std::string> ids = getIdentifiers(nodeText);
      for (const auto &id : ids) {
        auto  resolved = user_id_to_username_fn_(id);
        std::string replacement = "%{" + id + "}";
        replaced_identifiers_[id] = resolved;
        replaced_identifiers_[replacement] = resolved;
        nodeText = utils::string::replaceAll(nodeText, replacement, resolved);
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
          auto resolved = windows_event_log_metadata_.getEventData(it->second, input);
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

std::vector<std::string> MetadataWalker::getIdentifiers(const std::string &text) {
  auto pos = text.find("%{");
  std::vector<std::string> found_strings;
  while (pos != std::string::npos) {
    auto next_pos = text.find('}', pos);
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

std::string MetadataWalker::getMetadata(Metadata metadata) const {
  static constexpr auto get_from_map = [](const std::map<std::string, std::string>& map, const std::string& field) {
    auto it = map.find(field);
    if (it != std::end(map)) {
      return it->second;
    }
    return std::string{"N/A"};
  };

  switch (metadata) {
    case Metadata::LOG_NAME:
      return log_name_;
    case Metadata::SOURCE:
      return get_from_map(metadata_, "Provider");
    case Metadata::TIME_CREATED:
      return windows_event_log_metadata_.getEventTimestamp();
    case Metadata::EVENTID:
      return get_from_map(metadata_, "EventID");
    case Metadata::EVENT_RECORDID:
      return get_from_map(metadata_, "EventRecordID");
    case Metadata::OPCODE:
      return get_from_map(metadata_, "Opcode");
    case Metadata::TASK_CATEGORY:
      return get_from_map(metadata_, "Task");
    case Metadata::LEVEL:
      return get_from_map(metadata_, "Level");
    case Metadata::KEYWORDS:
      return get_from_map(metadata_, "Keywords");
    case Metadata::EVENT_TYPE:
      return std::to_string(windows_event_log_metadata_.getEventTypeIndex());
    case Metadata::COMPUTER:
      return WindowsEventLogMetadata::getComputerName();
    case Metadata::USER:
      break;
  }
  return "N/A";
}

std::map<std::string, std::string> MetadataWalker::getFieldValues() const {
  return fields_values_;
}

std::map<std::string, std::string> MetadataWalker::getIdentifiers() const {
  return replaced_identifiers_;
}

template<typename Fn>
requires std::is_convertible_v<std::invoke_result_t<Fn, std::string>, std::string>
void MetadataWalker::updateText(pugi::xml_node &node, const std::string &field_name, Fn &&fn) {
  std::string previous_value = node.text().get();
  auto new_field_value = std::invoke(std::forward<Fn>(fn), previous_value);
  if (new_field_value != previous_value) {
    metadata_[field_name] = new_field_value;
    if (update_xml_) {
      node.text().set(new_field_value.c_str());
    } else {
      fields_values_[field_name] = new_field_value;
    }
  }
}

template<typename Fn>
requires std::is_convertible_v<std::invoke_result_t<Fn, std::string>, std::string>
void MetadataWalker::updateAttributeValue(pugi::xml_attribute &attr, const std::string &field_name, Fn &&fn) {
  std::string previous_value = attr.value();
  auto new_field_value = std::invoke(std::forward<Fn>(fn), previous_value);
  if (new_field_value != previous_value) {
    metadata_[field_name] = new_field_value;
    if (update_xml_) {
      attr.set_value(new_field_value.c_str());
    } else {
      fields_values_[field_name] = new_field_value;
    }
  }
}

}  // namespace org::apache::nifi::minifi::wel
