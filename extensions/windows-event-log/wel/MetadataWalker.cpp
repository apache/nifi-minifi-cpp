#include <windows.h>
#include "MetadataWalker.h"
#include "XMLString.h"
#include <strsafe.h>

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
      static const auto idUpdate = [&](const std::string &input) {
        if (resolve_) {
          const auto resolved = utils::OsUtils::userIdToUsername(input);
          replaced_identifiers_[input] = resolved;
          return resolved;
        }

        replaced_identifiers_[input] = input;
        return input;
      };

      if (std::regex_match(attr.name(), regex_)) {
        updateText(node, attr.name(), idUpdate);
      }

      if (std::regex_match(attr.value(), regex_)) {
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

  }
  else if (node_name == "TimeCreated") {
    metadata_["TimeCreated"] = node.attribute("SystemTime").value();
  }
  else if (node_name == "EventRecordID") {
    metadata_["EventRecordID"] = node.text().get();
  }
  else if (node_name == "Provider") {
    metadata_["Provider"] = node.attribute("Name").value();
  }
  else if (node_name == "EventID") {
    metadata_["EventID"] = node.text().get();
  }
  else {
    static std::map<std::string, EVT_FORMAT_MESSAGE_FLAGS> formatFlagMap = { {"Channel", EvtFormatMessageChannel}, {"Keywords", EvtFormatMessageKeyword}, {"Level", EvtFormatMessageLevel}, {"Opcode", EvtFormatMessageOpcode}, {"Task",EvtFormatMessageTask} };
    auto it = formatFlagMap.find(node_name);
    if (it != formatFlagMap.end()) {
      std::function<std::string(const std::string &)> updateFunc = [&](const std::string &input) -> std::string {
        if (resolve_) {
          auto resolved = getEventData(it->second);
          if (!resolved.empty()) {
            return resolved;
          }
        }
        return input;
      };
      updateText(node, node.name(), std::move(updateFunc));
    }
    else {
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
      if (potential_identifier.find("S-") != std::string::npos){
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
        return getString(metadata_,"Provider");
      case TIME_CREATED:
        return event_timestamp_str_;
      case EVENTID:
        return getString(metadata_,"EventID");
      case EVENT_RECORDID:
        return getString(metadata_, "EventRecordID");
      case OPCODE:
        return getString(metadata_, "Opcode");
      case TASK_CATEGORY:
        return getString(metadata_,"Task");
      case LEVEL:
        return getString(metadata_,"Level");
      case KEYWORDS:
        return getString(metadata_,"Keywords");
      case EVENT_TYPE:
        return std::to_string(event_type_index_);
      case COMPUTER:
        return getComputerName();
    };
    return "N/A";
}

std::map<std::string, std::string> MetadataWalker::getFieldValues() const {
  return fields_values_;
}

std::map<std::string, std::string> MetadataWalker::getIdentifiers() const {
	return replaced_identifiers_;
}

std::string MetadataWalker::updateXmlMetadata(const std::string &xml, EVT_HANDLE metadata_ptr, EVT_HANDLE event_ptr, bool update_xml, bool resolve, const std::string &regex) {
  MetadataWalker walker(metadata_ptr,"", event_ptr, update_xml, resolve, regex);

  pugi::xml_document doc;
  pugi::xml_parse_result result = doc.load_string(xml.c_str());

  if (result) {
    doc.traverse(walker);
    wel::XmlString writer;
    doc.print(writer, "", pugi::format_raw); // no indentation or formatting
    return writer.xml_;
  }
  else {
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

std::string MetadataWalker::getEventData(EVT_FORMAT_MESSAGE_FLAGS flags) {
  LPWSTR string_buffer = NULL;
  DWORD string_buffer_size = 0;
  DWORD string_buffer_used = 0;
  DWORD result = 0;

  std::string event_data;

  if (metadata_ptr_ == NULL | event_ptr_ == NULL) {
    return event_data;
  }
  if (!EvtFormatMessage(metadata_ptr_, event_ptr_, 0, 0, NULL, flags, string_buffer_size, string_buffer, &string_buffer_used)) {
    result = GetLastError();
    if (ERROR_INSUFFICIENT_BUFFER == result) {
      string_buffer_size = string_buffer_used;

      string_buffer = (LPWSTR) malloc(string_buffer_size * sizeof(WCHAR));

      if (string_buffer) {

        if ((EvtFormatMessageKeyword == flags))
          string_buffer[string_buffer_size - 1] = L'\0';

        EvtFormatMessage(metadata_ptr_, event_ptr_, 0, 0, NULL, flags, string_buffer_size, string_buffer, &string_buffer_used);
        if ((EvtFormatMessageKeyword == flags))
          string_buffer[string_buffer_used - 1] = L'\0';
        std::wstring str(string_buffer);
        event_data = std::string(str.begin(), str.end());
        free(string_buffer);
      }

    }
  }
  return event_data;
}

} /* namespace wel */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
