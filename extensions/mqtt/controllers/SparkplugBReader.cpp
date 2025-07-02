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
#include "SparkplugBReader.h"

#include <vector>
#include <optional>

#include "google/protobuf/message.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/reflection.h"

#include "sparkplug_b.pb.h"
#include "minifi-cpp/core/Record.h"
#include "core/Resource.h"
#include "utils/expected.h"

namespace org::apache::nifi::minifi::controllers {

namespace {

nonstd::expected<core::RecordObject, std::error_code> parseRecordFromProtobufMessage(const google::protobuf::Message& message) {
  core::RecordObject result;
  const google::protobuf::Descriptor* descriptor = message.GetDescriptor();
  const google::protobuf::Reflection* reflection = message.GetReflection();

  int field_count = descriptor->field_count();
  for (int i = 0; i < field_count; ++i) {
    const google::protobuf::FieldDescriptor* field = descriptor->field(i);

    std::string full_name{field->name()};

    if (field->is_repeated()) {
      core::RecordArray record_array;
      int field_size = reflection->FieldSize(message, field);
      for (int j = 0; j < field_size; ++j) {
        switch (field->cpp_type()) {
          case google::protobuf::FieldDescriptor::CPPTYPE_INT32:
            record_array.push_back(core::RecordField(reflection->GetRepeatedInt32(message, field, j)));
            break;
          case google::protobuf::FieldDescriptor::CPPTYPE_INT64:
            record_array.push_back(core::RecordField(reflection->GetRepeatedInt64(message, field, j)));
            break;
          case google::protobuf::FieldDescriptor::CPPTYPE_UINT32:
            record_array.push_back(core::RecordField(reflection->GetRepeatedUInt32(message, field, j)));
            break;
          case google::protobuf::FieldDescriptor::CPPTYPE_UINT64:
            record_array.push_back(core::RecordField(reflection->GetRepeatedUInt64(message, field, j)));
            break;
          case google::protobuf::FieldDescriptor::CPPTYPE_DOUBLE:
            record_array.push_back(core::RecordField(reflection->GetRepeatedDouble(message, field, j)));
            break;
          case google::protobuf::FieldDescriptor::CPPTYPE_FLOAT:
            record_array.push_back(core::RecordField(reflection->GetRepeatedFloat(message, field, j)));
            break;
          case google::protobuf::FieldDescriptor::CPPTYPE_BOOL:
            record_array.push_back(core::RecordField(reflection->GetRepeatedBool(message, field, j)));
            break;
          case google::protobuf::FieldDescriptor::CPPTYPE_STRING:
            record_array.push_back(core::RecordField(reflection->GetRepeatedString(message, field, j)));
            break;
          case google::protobuf::FieldDescriptor::CPPTYPE_MESSAGE: {
            auto parse_result = parseRecordFromProtobufMessage(reflection->GetRepeatedMessage(message, field, j));
            if (!parse_result) {
              return nonstd::make_unexpected(parse_result.error());
            }
            record_array.push_back(core::RecordField{*parse_result});
            break;
          }
          default:
            return nonstd::make_unexpected(std::make_error_code(std::errc::invalid_argument));
        }
      }
      result.emplace(full_name, core::RecordField(record_array));
    } else if (reflection->HasField(message, field)) {
      switch (field->cpp_type()) {
        case google::protobuf::FieldDescriptor::CPPTYPE_INT32:
          result.emplace(full_name, core::RecordField(reflection->GetInt32(message, field)));
          break;
        case google::protobuf::FieldDescriptor::CPPTYPE_INT64:
          result.emplace(full_name, core::RecordField(reflection->GetInt64(message, field)));
          break;
        case google::protobuf::FieldDescriptor::CPPTYPE_UINT32:
          result.emplace(full_name, core::RecordField(reflection->GetUInt32(message, field)));
          break;
        case google::protobuf::FieldDescriptor::CPPTYPE_UINT64:
          result.emplace(full_name, core::RecordField(reflection->GetUInt64(message, field)));
          break;
        case google::protobuf::FieldDescriptor::CPPTYPE_DOUBLE:
          result.emplace(full_name, core::RecordField(reflection->GetDouble(message, field)));
          break;
        case google::protobuf::FieldDescriptor::CPPTYPE_FLOAT:
          result.emplace(full_name, core::RecordField(reflection->GetFloat(message, field)));
          break;
        case google::protobuf::FieldDescriptor::CPPTYPE_BOOL:
          result.emplace(full_name, core::RecordField(reflection->GetBool(message, field)));
          break;
        case google::protobuf::FieldDescriptor::CPPTYPE_STRING:
          result.emplace(full_name, core::RecordField(reflection->GetString(message, field)));
          break;
        case google::protobuf::FieldDescriptor::CPPTYPE_MESSAGE: {
          auto parse_result = parseRecordFromProtobufMessage(reflection->GetMessage(message, field));
          if (!parse_result) {
            return nonstd::make_unexpected(parse_result.error());
          }
          result.emplace(full_name, core::RecordField{*parse_result});
          break;
        }
        default:
          return nonstd::make_unexpected(std::make_error_code(std::errc::invalid_argument));
      }
    }
  }
  return result;
}

}  // namespace

nonstd::expected<core::RecordSet, std::error_code> SparkplugBReader::read(io::InputStream& input_stream) {
  org::eclipse::tahu::protobuf::Payload payload;
  std::vector<std::byte> buffer(input_stream.size());
  auto result = input_stream.read(buffer);

  if (io::isError(result) || result != input_stream.size()) {
    return nonstd::make_unexpected(std::make_error_code(std::errc::invalid_argument));
  }

  if (!payload.ParseFromArray(static_cast<void*>(buffer.data()), gsl::narrow<int>(input_stream.size()))) {
    return nonstd::make_unexpected(std::make_error_code(std::errc::invalid_argument));
  }

  auto parse_result = parseRecordFromProtobufMessage(payload);
  if (!parse_result) {
    logger_->log_error("Failed to parse Sparkplug payload: {}", parse_result.error().message());
    return nonstd::make_unexpected(parse_result.error());
  }
  core::Record record{std::move(*parse_result)};
  core::RecordSet record_set;
  record_set.push_back(std::move(record));
  return record_set;
}

REGISTER_RESOURCE(SparkplugBReader, ControllerService);

}  // namespace org::apache::nifi::minifi::controllers
