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

#include <vector>
#include <string>
#include <functional>

#include <pugixml.hpp>

#ifdef GetObject
#undef GetObject
#endif
#include "rapidjson/document.h"
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/prettywriter.h"

#include "gsl/gsl-lite.hpp";

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace wel {

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
      correlation.AddMember("ActivityID", rapidjson::StringRef(correlation_xml.attribute("ActivityID").value()), doc.GetAllocator());
    }

    {
      auto execution_xml = system_xml.child("Execution");
      auto& execution = flatten ? doc : system.AddMember("Execution", rapidjson::kObjectType, doc.GetAllocator())["Execution"];
      execution.AddMember("ProcessID", rapidjson::StringRef(execution_xml.attribute("ProcessID").value()), doc.GetAllocator());
      execution.AddMember("ThreadID", rapidjson::StringRef(execution_xml.attribute("ThreadID").value()), doc.GetAllocator());
    }

    system.AddMember("Channel", rapidjson::StringRef(system_xml.child("Channel").text().get()), doc.GetAllocator());
    system.AddMember("Computer", rapidjson::StringRef(system_xml.child("Computer").text().get()), doc.GetAllocator());
  }

  {
    auto eventData_xml = event_xml.child("EventData");
    // create EventData subarray even if flatten requested
    doc.AddMember("EventData", rapidjson::kArrayType, doc.GetAllocator());
    for (const auto& data : eventData_xml.children()) {
      auto name_attr = data.attribute("Name");
      rapidjson::Value item(rapidjson::kObjectType);
      item.AddMember("Name", rapidjson::StringRef(name_attr.value()), doc.GetAllocator());
      item.AddMember("Content", rapidjson::StringRef(data.text().get()), doc.GetAllocator());
      item.AddMember("Type", rapidjson::StringRef(data.name()), doc.GetAllocator());
      // we need to query EventData because a reference to it wouldn't be stable, as we
      // possibly add members to its parent which could result in reallocation
      doc["EventData"].PushBack(item, doc.GetAllocator());
      // check collision
      if (flatten && !name_attr.empty() && !doc.HasMember(name_attr.value())) {
        doc.AddMember(rapidjson::StringRef(name_attr.value()), rapidjson::StringRef(data.text().get()), doc.GetAllocator());
      }
    }
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

}  // namespace wel
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
