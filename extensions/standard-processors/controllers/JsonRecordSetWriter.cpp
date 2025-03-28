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

#include "JsonRecordSetWriter.h"

#include <rapidjson/prettywriter.h>
#include "core/Resource.h"
#include "utils/ProcessorConfigUtils.h"

namespace org::apache::nifi::minifi::standard {

namespace {

template<typename RecordFieldType>
rapidjson::Value toJson(const RecordFieldType& field, rapidjson::Document::AllocatorType&) {
  return rapidjson::Value(field);
};

template<>
rapidjson::Value toJson(const std::string& field, rapidjson::Document::AllocatorType& alloc) {
  return {field.c_str(), gsl::narrow<rapidjson::SizeType>(field.length()), alloc};
}

template<>
rapidjson::Value toJson(const std::chrono::system_clock::time_point& field, rapidjson::Document::AllocatorType& alloc) {
  const std::string serialized_time_point = utils::timeutils::getDateTimeStr(std::chrono::floor<std::chrono::seconds>(field));
  return {serialized_time_point.c_str(), gsl::narrow<rapidjson::SizeType>(serialized_time_point.length()), alloc};
}

template<>
rapidjson::Value toJson(const core::RecordArray& field, rapidjson::Document::AllocatorType&);

template<>
rapidjson::Value toJson(const core::RecordObject& field, rapidjson::Document::AllocatorType&);

template<>
rapidjson::Value toJson(const core::RecordArray& field, rapidjson::Document::AllocatorType& alloc) {
  auto array_json = rapidjson::Value(rapidjson::kArrayType);
  for (const auto& record : field) {
    auto json_value = (std::visit([&alloc](auto&& f)-> rapidjson::Value{ return toJson(f, alloc); }, record.value_));
    array_json.PushBack(json_value, alloc);
  }
  return array_json;
}

template<>
rapidjson::Value toJson(const core::RecordObject& field, rapidjson::Document::AllocatorType& alloc) {
  auto object_json = rapidjson::Value(rapidjson::kObjectType);
  for (const auto& [record_name, record_value] : field) {
    auto json_value = (std::visit([&alloc](auto&& f)-> rapidjson::Value{ return toJson(f, alloc); }, record_value.field->value_));
    rapidjson::Value json_name(record_name.c_str(), gsl::narrow<rapidjson::SizeType>(record_name.length()), alloc);
    object_json.AddMember(json_name, json_value, alloc);
  }
  return object_json;
}
}  // namespace

void JsonRecordSetWriter::onEnable() {
  output_grouping_ = getProperty(OutputGrouping.name) | utils::andThen(parsing::parseEnum<OutputGroupingType>) | utils::orThrow("JsonRecordSetWriter::OutputGrouping is required property");
  pretty_print_ = getProperty(PrettyPrint.name) | utils::andThen(parsing::parseBool) | utils::orThrow("Missing JsonRecordSetWriter::PrettyPrint despite default value");
}

void JsonRecordSetWriter::writePerLine(const core::RecordSet& record_set, const std::shared_ptr<core::FlowFile>& flow_file, core::ProcessSession& session) {
  session.write(flow_file, [&record_set](const std::shared_ptr<io::OutputStream>& stream) -> int64_t {
    int64_t write_result = 0;
    for (const auto& record : record_set) {
      auto doc = rapidjson::Document(rapidjson::kObjectType);
      auto& allocator = doc.GetAllocator();
      convertRecord(record, doc, allocator);
      rapidjson::StringBuffer buffer;
      rapidjson::Writer writer(buffer);
      doc.Accept(writer);
      write_result += gsl::narrow<int64_t>(stream->write(gsl::make_span(fmt::format("{}\n", buffer.GetString())).as_span<const std::byte>()));
    }
    return write_result;
  });
}

void JsonRecordSetWriter::writeAsArray(const core::RecordSet& record_set, const std::shared_ptr<core::FlowFile>& flow_file, core::ProcessSession& session) const {
  session.write(flow_file, [this, &record_set](const std::shared_ptr<io::OutputStream>& stream) -> int64_t {
    auto doc = rapidjson::Document(rapidjson::kArrayType);
    for (const auto& record : record_set) {
      auto& allocator = doc.GetAllocator();
      auto record_json = rapidjson::Value(rapidjson::kObjectType);
      convertRecord(record, record_json, allocator);
      doc.PushBack(std::move(record_json), allocator);
    }
    rapidjson::StringBuffer buffer;
    if (pretty_print_) {
      rapidjson::PrettyWriter pretty_writer(buffer);
      doc.Accept(pretty_writer);
    } else {
      rapidjson::Writer writer(buffer);
      doc.Accept(writer);
    }
    return gsl::narrow<int64_t>(stream->write(gsl::make_span(fmt::format("{}", buffer.GetString())).as_span<const std::byte>()));
  });
}

void JsonRecordSetWriter::write(const core::RecordSet& record_set, const std::shared_ptr<core::FlowFile>& flow_file, core::ProcessSession& session) {
  switch (output_grouping_) {
    case OutputGroupingType::ARRAY:
      return writeAsArray(record_set, flow_file, session);
    case OutputGroupingType::ONE_LINE_PER_OBJECT:
        return writePerLine(record_set, flow_file, session);
  }
}

void JsonRecordSetWriter::convertRecord(const core::Record& record, rapidjson::Value& record_json, rapidjson::Document::AllocatorType& alloc) {
  for (const auto& [field_name, field_val] : record) {
    rapidjson::Value json_name(field_name.c_str(), gsl::narrow<rapidjson::SizeType>(field_name.length()), alloc);
    rapidjson::Value json_value = (std::visit([&alloc](auto&& f)-> rapidjson::Value{ return toJson(f, alloc); }, field_val.value_));
    record_json.AddMember(json_name, json_value, alloc);
  }
}

REGISTER_RESOURCE(JsonRecordSetWriter, ControllerService);

}  // namespace org::apache::nifi::minifi::standard
