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

#include "JsonRecordSetReader.h"

#ifdef WIN32
#pragma push_macro("GetObject")
#undef GetObject  // windows.h #defines GetObject = GetObjectA or GetObjectW, which conflicts with rapidjson
#endif

namespace org::apache::nifi::minifi::standard {

namespace {

nonstd::expected<core::RecordField, std::error_code> parse(const rapidjson::Value& json_value) {
  if (json_value.IsDouble()) {
    return core::RecordField{json_value.GetDouble()};
  }
  if (json_value.IsBool()) {
    return core::RecordField{json_value.GetBool()};
  }
  if (json_value.IsInt64()) {
    return core::RecordField{json_value.GetInt64()};
  }
  if (json_value.IsString()) {
    return core::RecordField{json_value.GetString()};
  }
  if (json_value.IsArray()) {
    core::RecordArray record_array;
    for (const auto& element : json_value.GetArray()) {
      auto element_field = parse(element);
      if (!element_field)
        return nonstd::make_unexpected(element_field.error());
      record_array.push_back(std::move(*element_field));
    }
    return core::RecordField{std::move(record_array)};
  }
  if (json_value.IsObject()) {
    core::RecordObject record_object;
    for (const auto& m : json_value.GetObject()) {
      auto element_key = m.name.GetString();
      auto element_field = parse(m.value);
      if (!element_field)
        return nonstd::make_unexpected(element_field.error());
      record_object[element_key] = core::BoxedRecordField{std::make_unique<core::RecordField>(std::move(*element_field))};
    }
    return core::RecordField{std::move(record_object)};
  }

  return nonstd::make_unexpected(std::make_error_code(std::errc::invalid_argument));
}

nonstd::expected<core::Record, std::error_code> parseRecord(rapidjson::Value& record_json) {
  core::Record result;
  if (!record_json.IsObject()) {
    return nonstd::make_unexpected(std::make_error_code(std::errc::invalid_argument));
  }
  for (const auto& member : record_json.GetObject()) {
    const auto element_key = member.name.GetString();
    auto element_field = parse(member.value);
    if (!element_field)
      return nonstd::make_unexpected(element_field.error());
    result.emplace(element_key, std::move(*element_field));
  }
  return result;
}
}  // namespace

bool readAsJsonLines(const std::string& content, core::RecordSet& record_set) {
  std::stringstream ss(content);
  std::string line;
  while (std::getline(ss, line, '\n')) {
    rapidjson::Document document;
    if (rapidjson::ParseResult parse_result = document.Parse<rapidjson::kParseStopWhenDoneFlag>(line); parse_result.IsError())
      return false;
    auto record = parseRecord(document);
    if (!record)
      return false;
    record_set.push_back(std::move(*record));
  }
  return true;
}

bool readAsArray(const std::string& content, core::RecordSet& record_set) {
  rapidjson::Document document;
  if (const rapidjson::ParseResult parse_result = document.Parse<rapidjson::kParseStopWhenDoneFlag>(content); parse_result.IsError())
    return false;
  if (!document.IsArray())
    return false;
  for (auto& json_record : document.GetArray()) {
    auto record  = parseRecord(json_record);
    if (!record)
      return false;
    record_set.push_back(std::move(*record));
  }
  return true;
}

nonstd::expected<core::RecordSet, std::error_code> JsonRecordSetReader::read(const std::shared_ptr<core::FlowFile>& flow_file, core::ProcessSession& session) {
  core::RecordSet record_set{};
  const auto read_result = session.read(flow_file, [&record_set](const std::shared_ptr<io::InputStream>& input_stream) -> int64_t {
    std::string content;
    content.resize(input_stream->size());
    const auto read_ret = gsl::narrow<int64_t>(input_stream->read(as_writable_bytes(std::span(content))));
    if (io::isError(read_ret)) {
      return -1;
    }
    if (content.starts_with('[')) {
      readAsArray(content, record_set);
    } else {
      readAsJsonLines(content, record_set);
    }
    return read_ret;
  });
  if (io::isError(read_result))
    return nonstd::make_unexpected(std::make_error_code(std::errc::invalid_argument));
  return record_set;
}

}  // namespace org::apache::nifi::minifi::standard

#ifdef WIN32
#pragma pop_macro("GetObject")
#endif
