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
#include "XMLRecordSetWriter.h"

#include "core/Resource.h"
#include "Exception.h"
#include "utils/TimeUtil.h"

namespace org::apache::nifi::minifi::standard {

void XMLRecordSetWriter::onEnable() {
  if (auto wrap_elements_of_arrays = magic_enum::enum_cast<WrapElementsOfArraysOptions>(getProperty(WrapElementsOfArrays.name).value_or("No Wrapping"))) {
    wrap_elements_of_arrays_ = *wrap_elements_of_arrays;
  } else {
    throw Exception(PROCESS_SCHEDULE_EXCEPTION, "Invalid value for Wrap Elements of Arrays property: " + getProperty(WrapElementsOfArrays.name).value_or(""));
  }

  array_tag_name_ = getProperty(ArrayTagName.name).value_or("");
  if (array_tag_name_.empty() &&
      (wrap_elements_of_arrays_ == WrapElementsOfArraysOptions::UsePropertyAsWrapper ||
       wrap_elements_of_arrays_ == WrapElementsOfArraysOptions::UsePropertyForElements)) {
    throw Exception(PROCESS_SCHEDULE_EXCEPTION, "Array Tag Name property must be set when Wrap Elements of Arrays is set to Use Property as Wrapper or Use Property for Elements");
  }

  omit_xml_declaration_ = getProperty(OmitXMLDeclaration.name).value_or("false") == "true";
  pretty_print_xml_ = getProperty(PrettyPrintXML.name).value_or("false") == "true";

  name_of_record_tag_ = getProperty(NameOfRecordTag.name).value_or("");
  if (name_of_record_tag_.empty()) {
    throw Exception(PROCESS_SCHEDULE_EXCEPTION, "Name of Record Tag property must be set");
  }

  name_of_root_tag_ = getProperty(NameOfRootTag.name).value_or("");
  if (name_of_root_tag_.empty()) {
    throw Exception(PROCESS_SCHEDULE_EXCEPTION, "Name of Root Tag property must be set");
  }
}

std::string XMLRecordSetWriter::formatXmlOutput(pugi::xml_document& xml_doc) const {
  std::ostringstream xml_string_stream;
  uint64_t xml_formatting_flags = 0;
  if (pretty_print_xml_) {
    xml_formatting_flags |= pugi::format_indent;
  } else {
    xml_formatting_flags |= pugi::format_raw;
  }
  if (omit_xml_declaration_) {
    xml_formatting_flags |= pugi::format_no_declaration;
  }
  xml_doc.save(xml_string_stream, "  ", gsl::narrow<unsigned int>(xml_formatting_flags));
  return xml_string_stream.str();
}

void XMLRecordSetWriter::convertRecordArrayField(const std::string& field_name, const core::RecordField& field, pugi::xml_node& parent_node) const {
  const auto& record_array = std::get<core::RecordArray>(field.value_);
  pugi::xml_node array_node;
  if (wrap_elements_of_arrays_ == WrapElementsOfArraysOptions::UsePropertyAsWrapper) {
    array_node = parent_node.append_child(array_tag_name_.c_str());
  } else if (wrap_elements_of_arrays_ == WrapElementsOfArraysOptions::UsePropertyForElements) {
    array_node = parent_node.append_child(field_name.c_str());
  }
  for (const auto& array_field : record_array) {
    if (wrap_elements_of_arrays_ == WrapElementsOfArraysOptions::UsePropertyAsWrapper) {
      convertRecordField(field_name, array_field, array_node);
    } else if (wrap_elements_of_arrays_ == WrapElementsOfArraysOptions::UsePropertyForElements) {
      convertRecordField(array_tag_name_, array_field, array_node);
    } else {
      convertRecordField(field_name, array_field, parent_node);
    }
  }
}

void XMLRecordSetWriter::convertRecordField(const std::string& field_name, const core::RecordField& field, pugi::xml_node& parent_node) const {
  if (std::holds_alternative<core::RecordArray>(field.value_)) {
    convertRecordArrayField(field_name, field, parent_node);
    return;
  }

  pugi::xml_node field_node = parent_node.append_child(field_name.c_str());
  if (std::holds_alternative<std::string>(field.value_)) {
    field_node.text().set(std::get<std::string>(field.value_));
  } else if (std::holds_alternative<int64_t>(field.value_)) {
    field_node.text().set(std::to_string(std::get<int64_t>(field.value_)).c_str());
  } else if (std::holds_alternative<uint64_t>(field.value_)) {
    field_node.text().set(std::to_string(std::get<uint64_t>(field.value_)).c_str());
  } else if (std::holds_alternative<double>(field.value_)) {
    field_node.text().set(fmt::format("{:g}", std::get<double>(field.value_)).c_str());
  } else if (std::holds_alternative<bool>(field.value_)) {
    field_node.text().set(std::get<bool>(field.value_) ? "true" : "false");
  } else if (std::holds_alternative<std::chrono::system_clock::time_point>(field.value_)) {
    auto time_point = std::get<std::chrono::system_clock::time_point>(field.value_);
    auto time_str = utils::timeutils::getDateTimeStr(std::chrono::time_point_cast<std::chrono::seconds>(time_point));
    field_node.text().set(time_str.c_str());
  } else if (std::holds_alternative<core::RecordObject>(field.value_)) {
    const auto& record_object = std::get<core::RecordObject>(field.value_);
    for (const auto& [obj_key, obj_field] : record_object) {
      convertRecordField(obj_key, obj_field, field_node);
    }
  }
}

std::string XMLRecordSetWriter::convertRecordSetToXml(const core::RecordSet& record_set) const {
  gsl_Expects(!name_of_record_tag_.empty() && !name_of_root_tag_.empty());
  pugi::xml_document xml_doc;
  auto root_node = xml_doc.append_child(name_of_root_tag_.c_str());

  for (const auto& record : record_set) {
    auto record_node = root_node.append_child(name_of_record_tag_.c_str());
    for (const auto& [key, field] : record) {
      convertRecordField(key, field, record_node);
    }
  }

  return formatXmlOutput(xml_doc);
}

void XMLRecordSetWriter::write(const core::RecordSet& record_set, const std::shared_ptr<core::FlowFile>& flow_file, core::ProcessSession& session) {
  if (!flow_file) {
    logger_->log_error("FlowFile is null, cannot write RecordSet to XML");
    return;
  }

  auto xml_content = convertRecordSetToXml(record_set);
  session.write(flow_file, [&xml_content](const std::shared_ptr<io::OutputStream>& stream) -> int64_t {
    stream->write(reinterpret_cast<const uint8_t*>(xml_content.data()), xml_content.size());
    return gsl::narrow<int64_t>(xml_content.size());
  });
}

REGISTER_RESOURCE(XMLRecordSetWriter, ControllerService);
}  // namespace org::apache::nifi::minifi::standard
