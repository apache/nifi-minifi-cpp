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

#include "JSONUtils.h"

#include <algorithm>
#include <string>

#include <pugixml.hpp>

#include "rapidjson/document.h"
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"

#include "minifi-cpp/utils/gsl.h"
#include "utils/StringUtils.h"

namespace org::apache::nifi::minifi::wel {
namespace {
rapidjson::Value xmlElementToJSON(const pugi::xml_node& node, rapidjson::Document& doc) {
  gsl_Expects(node.type() == pugi::xml_node_type::node_element);
  rapidjson::Value object(rapidjson::kObjectType);
  object.AddMember("name", rapidjson::StringRef(node.name()), doc.GetAllocator());
  auto& attributes = object.AddMember("attributes", rapidjson::kObjectType, doc.GetAllocator())["attributes"];
  for (const auto& attr : node.attributes()) {
    attributes.AddMember(rapidjson::StringRef(attr.name()), rapidjson::StringRef(attr.value()), doc.GetAllocator());
  }
  auto& children = object.AddMember("children", rapidjson::kArrayType, doc.GetAllocator())["children"];
  for (const auto& child : node.children()) {
    if (child.type() == pugi::xml_node_type::node_element) {
      children.PushBack(xmlElementToJSON(child, doc), doc.GetAllocator());
    }
  }
  object.AddMember("text", rapidjson::StringRef(node.text().get()), doc.GetAllocator());
  return object;
}

rapidjson::Value xmlDocumentToJSON(const pugi::xml_node& node, rapidjson::Document& doc) {
  gsl_Expects(node.type() == pugi::xml_node_type::node_document);
  rapidjson::Value children(rapidjson::kArrayType);
  for (const auto& child : node.children()) {
    if (child.type() == pugi::xml_node_type::node_element) {
      children.PushBack(xmlElementToJSON(child, doc), doc.GetAllocator());
    }
  }
  return children;
}

void simplifiedGenericXmlToJson(const pugi::xml_node& source_node,
    rapidjson::Value& output_value,
    rapidjson::Document::AllocatorType& allocator,
    std::optional<std::string> prefix_for_flat_structure) {
  gsl_Expects(source_node.type() == pugi::xml_node_type::node_element);
  const bool is_flattened = prefix_for_flat_structure.has_value();
  for (const auto& attr : source_node.attributes()) {
    if (attr.name() == std::string_view{"xmlns"}) {
      continue;  // skip xmlns attribute, because it's metadata
    }
    if (!is_flattened) {
      output_value.AddMember(rapidjson::StringRef(attr.name()), rapidjson::StringRef(attr.value()), allocator);
    } else {
      output_value.AddMember(rapidjson::Value(*prefix_for_flat_structure + attr.name(), allocator).Move(), rapidjson::StringRef(attr.value()), allocator);
    }
  }
  for (const auto& child: source_node.children()) {
    if (child.type() == pugi::xml_node_type::node_element) {
      const auto is_pcdata = [](const pugi::xml_node& node) { return node.type() == pugi::xml_node_type::node_pcdata; };
      if (std::all_of(child.children().begin(), child.children().end(), is_pcdata)) {
        // all children are pcdata (text): leaf node
        if (!is_flattened) {
          output_value.AddMember(rapidjson::StringRef(child.name()), rapidjson::StringRef(child.text().get()), allocator);
        } else {
          output_value.AddMember(rapidjson::Value(*prefix_for_flat_structure + child.name(), allocator).Move(), rapidjson::StringRef(child.text().get()), allocator);
        }
      } else {
        // there are non-text children: recurse further
        auto& child_val = is_flattened ? output_value : output_value.AddMember(rapidjson::StringRef(child.name()), rapidjson::kObjectType, allocator)[child.name()];
        auto new_prefix = is_flattened ? std::optional(*prefix_for_flat_structure + child.name() + ".") : std::nullopt;
        simplifiedGenericXmlToJson(child, child_val, allocator, new_prefix);
      }
    }
  }
}

std::string createUniqueKey(const std::string& key, const rapidjson::Value& parent) {
  auto proposed_key = key;
  size_t postfix = 1;
  while (parent.HasMember(proposed_key)) {
    proposed_key = key + std::to_string(postfix++);
  }
  return proposed_key;
}

rapidjson::Document toJSONImpl(const pugi::xml_node& root, bool flatten) {
  rapidjson::Document doc{rapidjson::kObjectType};

  auto event_xml = root.child("Event");

  {
    auto system_xml = event_xml.child("System");
    auto& system = flatten ? doc : doc.AddMember("System", rapidjson::kObjectType, doc.GetAllocator())["System"];

    {
      auto provider_xml = system_xml.child("Provider");
      auto& provider = flatten ? doc : system.AddMember("Provider", rapidjson::kObjectType, doc.GetAllocator())["Provider"];
      provider.AddMember("Name", rapidjson::StringRef(provider_xml.attribute("Name").value()), doc.GetAllocator());
      provider.AddMember("Guid", rapidjson::StringRef(provider_xml.attribute("Guid").value()), doc.GetAllocator());
    }

    system.AddMember("EventID", rapidjson::StringRef(system_xml.child("EventID").text().get()), doc.GetAllocator());
    system.AddMember("Version", rapidjson::StringRef(system_xml.child("Version").text().get()), doc.GetAllocator());
    system.AddMember("Level", rapidjson::StringRef(system_xml.child("Level").text().get()), doc.GetAllocator());
    system.AddMember("Task", rapidjson::StringRef(system_xml.child("Task").text().get()), doc.GetAllocator());
    system.AddMember("Opcode", rapidjson::StringRef(system_xml.child("Opcode").text().get()), doc.GetAllocator());
    system.AddMember("Keywords", rapidjson::StringRef(system_xml.child("Keywords").text().get()), doc.GetAllocator());

    {
      auto timeCreated_xml = system_xml.child("TimeCreated");
      auto& timeCreated = flatten ? doc : system.AddMember("TimeCreated", rapidjson::kObjectType, doc.GetAllocator())["TimeCreated"];
      timeCreated.AddMember("SystemTime", rapidjson::StringRef(timeCreated_xml.attribute("SystemTime").value()), doc.GetAllocator());
    }

    system.AddMember("EventRecordID", rapidjson::StringRef(system_xml.child("EventRecordID").text().get()), doc.GetAllocator());

    {
      auto correlation_xml = system_xml.child("Correlation");
      auto& correlation = flatten ? doc : system.AddMember("Correlation", rapidjson::kObjectType, doc.GetAllocator())["Correlation"];
      const auto activity_id = correlation_xml.attribute("ActivityID");
      if (!activity_id.empty()) {
        correlation.AddMember("ActivityID", rapidjson::StringRef(activity_id.value()), doc.GetAllocator());
      }
    }

    {
      auto execution_xml = system_xml.child("Execution");
      auto& execution = flatten ? doc : system.AddMember("Execution", rapidjson::kObjectType, doc.GetAllocator())["Execution"];
      execution.AddMember("ProcessID", rapidjson::StringRef(execution_xml.attribute("ProcessID").value()), doc.GetAllocator());
      execution.AddMember("ThreadID", rapidjson::StringRef(execution_xml.attribute("ThreadID").value()), doc.GetAllocator());
    }

    system.AddMember("Channel", rapidjson::StringRef(system_xml.child("Channel").text().get()), doc.GetAllocator());
    system.AddMember("Computer", rapidjson::StringRef(system_xml.child("Computer").text().get()), doc.GetAllocator());

    {
      auto security_xml = system_xml.child("Security");
      auto& security = flatten ? doc : system.AddMember("Security", rapidjson::kObjectType, doc.GetAllocator())["Security"];
      security.AddMember("UserID", rapidjson::StringRef(security_xml.attribute("UserID").value()), doc.GetAllocator());
    }
  }

  {
    auto eventData_xml = event_xml.child("EventData");
    if (flatten) {
      for (const auto& event_data_child : eventData_xml.children()) {
        std::string key = "EventData";
        if (auto name_attr = event_data_child.attribute("Name"); !name_attr.empty()) {
          key = utils::string::join_pack(key, ".", name_attr.value());
        }

        doc.AddMember(rapidjson::Value(createUniqueKey(key, doc), doc.GetAllocator()).Move(), rapidjson::StringRef(event_data_child.text().get()), doc.GetAllocator());
      }
    } else {
      doc.AddMember("EventData", rapidjson::kArrayType, doc.GetAllocator());
      for (const auto& event_data_child : eventData_xml.children()) {
        auto name_attr = event_data_child.attribute("Name");
        rapidjson::Value item(rapidjson::kObjectType);
        item.AddMember("Name", rapidjson::StringRef(name_attr.value()), doc.GetAllocator());
        item.AddMember("Content", rapidjson::StringRef(event_data_child.text().get()), doc.GetAllocator());
        item.AddMember("Type", rapidjson::StringRef(event_data_child.name()), doc.GetAllocator());
        doc["EventData"].PushBack(item, doc.GetAllocator());  // we need to re-query EventData because a reference to it wouldn't be stable
      }
    }
  }

  const auto userdata_xml = event_xml.child("UserData");
  if (!userdata_xml.empty()) {
    auto& userdata = flatten ? doc : doc.AddMember("UserData", rapidjson::kObjectType, doc.GetAllocator())["UserData"];
    auto prefix = flatten ? std::optional("UserData.") : std::nullopt;
    simplifiedGenericXmlToJson(userdata_xml, userdata, doc.GetAllocator(), prefix);
  }

  return doc;
}

}  // namespace

rapidjson::Document toRawJSON(const pugi::xml_node& root) {
  rapidjson::Document doc;
  if (root.type() == pugi::xml_node_type::node_document) {
    static_cast<rapidjson::Value&>(doc) = xmlDocumentToJSON(root, doc);
  }
  return doc;
}

rapidjson::Document toSimpleJSON(const pugi::xml_node& root) {
  return toJSONImpl(root, false);
}

rapidjson::Document toFlattenedJSON(const pugi::xml_node& root) {
  return toJSONImpl(root, true);
}

std::string jsonToString(const rapidjson::Document& doc) {
  rapidjson::StringBuffer buffer;
  rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
  doc.Accept(writer);
  return buffer.GetString();
}
}  // namespace org::apache::nifi::minifi::wel
