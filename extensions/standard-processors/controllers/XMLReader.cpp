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

#include "XMLReader.h"

#include <algorithm>

#include "core/Resource.h"
#include "utils/TimeUtil.h"
#include "utils/gsl.h"

namespace org::apache::nifi::minifi::standard {

namespace {
bool hasChildNodes(const pugi::xml_node& node) {
  return std::any_of(node.begin(), node.end(), [] (const pugi::xml_node& child) {
    return child.type() == pugi::node_element;
  });
}

void addRecordFieldToObject(core::RecordObject& record_object, const std::string& name, const core::RecordField& field) {
  auto it = record_object.find(name);
  if (it == record_object.end()) {
    record_object.emplace(name, field);
    return;
  }

  if (std::holds_alternative<core::RecordArray>(it->second.value_)) {
    std::get<core::RecordArray>(it->second.value_).emplace_back(field);
    return;
  }

  core::RecordArray array;
  array.emplace_back(it->second);
  array.emplace_back(field);
  it->second = core::RecordField(std::move(array));
}
}  // namespace

void XMLReader::writeRecordField(core::RecordObject& record_object, const std::string& name, const std::string& value, bool write_pcdata_node) const {
  // If the name is the value set in the Field Name for Content property, we should only add this value to the RecordObject if we are writing a plain character data node.
  if (!write_pcdata_node && name == field_name_for_content_) {
    return;
  }

  if (value == "true" || value == "false") {
    addRecordFieldToObject(record_object, name, core::RecordField(value == "true"));
    return;
  } else if (auto date = utils::timeutils::parseDateTimeStr(value)) {
    addRecordFieldToObject(record_object, name, core::RecordField(*date));
    return;
  } else if (auto date = utils::timeutils::parseRfc3339(value)) {
    addRecordFieldToObject(record_object, name, core::RecordField(*date));
    return;
  }

  if (std::all_of(value.begin(), value.end(), ::isdigit)) {
    try {
      uint64_t value_as_uint64 = std::stoull(value);
      addRecordFieldToObject(record_object, name, core::RecordField(value_as_uint64));
      return;
    } catch (const std::exception&) {
    }
  }

  if (value.starts_with('-') && std::all_of(value.begin() + 1, value.end(), ::isdigit)) {
    try {
      int64_t value_as_int64 = std::stoll(value);
      addRecordFieldToObject(record_object, name, core::RecordField(value_as_int64));
      return;
    } catch (const std::exception&) {
    }
  }

  try {
    auto value_as_double = std::stod(value);
    addRecordFieldToObject(record_object, name, core::RecordField(value_as_double));
    return;
  } catch (const std::exception&) {
  }

  addRecordFieldToObject(record_object, name, core::RecordField(value));
}

void XMLReader::writeRecordFieldFromXmlNode(core::RecordObject& record_object, const pugi::xml_node& node) const {
  writeRecordField(record_object, node.name(), node.child_value());
}

void XMLReader::parseNodeElement(core::RecordObject& record_object, const pugi::xml_node& node) const {
  gsl_Expects(node.type() == pugi::node_element);
  if (parse_xml_attributes_ && node.first_attribute()) {
    core::RecordObject child_record_object;
    for (const pugi::xml_attribute& attr : node.attributes()) {
      writeRecordField(child_record_object, attribute_prefix_ + attr.name(), attr.value());
    }
    parseXmlNode(child_record_object, node);
    record_object.emplace(node.name(), core::RecordField(std::move(child_record_object)));
    return;
  }

  if (hasChildNodes(node)) {
    core::RecordObject child_record_object;
    parseXmlNode(child_record_object, node);
    record_object.emplace(node.name(), core::RecordField(std::move(child_record_object)));
    return;
  }

  writeRecordFieldFromXmlNode(record_object, node);
}

void XMLReader::parseXmlNode(core::RecordObject& record_object, const pugi::xml_node& node) const {
  std::string pc_data_value;
  for (pugi::xml_node child : node.children()) {
    if (child.type() == pugi::node_element) {
      parseNodeElement(record_object, child);
    } else if (child.type() == pugi::node_pcdata) {
      pc_data_value.append(child.value());
    }
  }

  if (!pc_data_value.empty()) {
    writeRecordField(record_object, field_name_for_content_, pc_data_value, true);
  }
}

void XMLReader::addRecordFromXmlNode(const pugi::xml_node& node, core::RecordSet& record_set) const {
  core::RecordObject record_object;
  parseXmlNode(record_object, node);
  core::Record record(std::move(record_object));
  record_set.emplace_back(std::move(record));
}

bool XMLReader::parseRecordsFromXml(core::RecordSet& record_set, const std::string& xml_content) const {
  pugi::xml_document doc;
  if (!doc.load_string(xml_content.c_str())) {
    logger_->log_error("Failed to parse XML content: {}", xml_content);
    return false;
  }

  if (expect_records_as_array_) {
    pugi::xml_node root = doc.first_child();
    for (pugi::xml_node record_node : root.children()) {
      addRecordFromXmlNode(record_node, record_set);
    }
    return true;
  }

  pugi::xml_node root = doc.first_child();
  if (!root.first_child()) {
    logger_->log_info("XML content does not contain any records: {}", xml_content);
    return true;
  }
  addRecordFromXmlNode(root, record_set);
  return true;
}

void XMLReader::onEnable() {
  field_name_for_content_ = getProperty(FieldNameForContent.name).value_or("value");
  parse_xml_attributes_ = getProperty(ParseXMLAttributes.name).value_or("false") == "true";
  attribute_prefix_ = getProperty(AttributePrefix.name).value_or("");
  expect_records_as_array_ = getProperty(ExpectRecordsAsArray.name).value_or("false") == "true";
}

nonstd::expected<core::RecordSet, std::error_code> XMLReader::read(io::InputStream& input_stream) {
  core::RecordSet record_set{};
  const auto read_result = [this, &record_set](io::InputStream& input_stream) -> int64_t {
    std::string content;
    content.resize(input_stream.size());
    const auto read_ret = gsl::narrow<int64_t>(input_stream.read(as_writable_bytes(std::span(content))));
    if (io::isError(read_ret)) {
      logger_->log_error("Failed to read XML data from input stream");
      return -1;
    }
    if (!parseRecordsFromXml(record_set, content)) {
      return -1;
    }
    return read_ret;
  }(input_stream);
  if (io::isError(read_result)) {
    return nonstd::make_unexpected(std::make_error_code(std::errc::invalid_argument));
  }
  return record_set;
}

REGISTER_RESOURCE(XMLReader, ControllerService);
}  // namespace org::apache::nifi::minifi::standard
