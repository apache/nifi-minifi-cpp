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

#include "JSONSQLWriter.h"
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/prettywriter.h"
#include "Exception.h"
#include "Utils.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace sql {

JSONSQLWriter::JSONSQLWriter(bool pretty)
  : pretty_(pretty), jsonPayload_(rapidjson::kArrayType) {
}

void JSONSQLWriter::beginProcessRow() {
  jsonRow_ = rapidjson::kObjectType;
}

void JSONSQLWriter::endProcessRow() {
  jsonPayload_.PushBack(jsonRow_, jsonPayload_.GetAllocator());
}

void JSONSQLWriter::beginProcessBatch() {
  jsonPayload_ = rapidjson::Document(rapidjson::kArrayType);
}

void JSONSQLWriter::endProcessBatch(State state) {}

void JSONSQLWriter::processColumnNames(const std::vector<std::string>& name) {}

void JSONSQLWriter::processColumn(const std::string& name, const std::string& value) {
  addToJSONRow(name, toJSONString(value));
}

void JSONSQLWriter::processColumn(const std::string& name, double value) {
  addToJSONRow(name, rapidjson::Value(value));
}

void JSONSQLWriter::processColumn(const std::string& name, int value) {
  addToJSONRow(name, rapidjson::Value(value));
}

void JSONSQLWriter::processColumn(const std::string& name, long long value) {
  addToJSONRow(name, rapidjson::Value(gsl::narrow<int64_t>(value)));
}

void JSONSQLWriter::processColumn(const std::string& name, unsigned long long value) {
  addToJSONRow(name, rapidjson::Value(gsl::narrow<uint64_t>(value)));
}

void JSONSQLWriter::processColumn(const std::string& name, const char* value) {
  addToJSONRow(name, toJSONString(value));
}

void JSONSQLWriter::addToJSONRow(const std::string& columnName, rapidjson::Value&& jsonValue) {
  jsonRow_.AddMember(toJSONString(columnName), std::move(jsonValue), jsonPayload_.GetAllocator());
}

rapidjson::Value JSONSQLWriter::toJSONString(const std::string& s) {
  rapidjson::Value jsonValue;
  jsonValue.SetString(s.c_str(), s.size(), jsonPayload_.GetAllocator());

  return jsonValue;
}

std::string JSONSQLWriter::toString() {
  rapidjson::StringBuffer buffer;

  if (pretty_) {
    rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);
    jsonPayload_.Accept(writer);
  } else {
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    jsonPayload_.Accept(writer);
  }

  return {buffer.GetString(), buffer.GetSize()};
}

}  // namespace sql
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
