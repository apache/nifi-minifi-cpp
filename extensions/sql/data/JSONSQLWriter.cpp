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

JSONSQLWriter::JSONSQLWriter()
  : jsonPayload_(rapidjson::kArrayType) {
}

JSONSQLWriter::~JSONSQLWriter() {}

void JSONSQLWriter::beginProcessRow() {
  jsonRow_ = rapidjson::kObjectType;
}

void JSONSQLWriter::endProcessRow() {
  jsonPayload_.PushBack(jsonRow_, jsonPayload_.GetAllocator());
}

void JSONSQLWriter::processColumnName(const std::string& name) {}

void JSONSQLWriter::processColumn(const std::string& name, const std::string& value) {
  addToJSONRow(name, toJSONString(value));
}

void JSONSQLWriter::processColumn(const std::string& name, double value) {
  addToJSONRow(name, rapidjson::Value().SetDouble(value));
}

void JSONSQLWriter::processColumn(const std::string& name, int value) {
  addToJSONRow(name, rapidjson::Value().SetInt(value));
}

void JSONSQLWriter::processColumn(const std::string& name, long long value) {
  addToJSONRow(name, rapidjson::Value().SetInt64(value));
}

void JSONSQLWriter::processColumn(const std::string& name, unsigned long long value) {
  addToJSONRow(name, rapidjson::Value().SetUint64(value));
}

void JSONSQLWriter::processColumn(const std::string& name, const char* value) {
  addToJSONRow(name, toJSONString(value));
}

void JSONSQLWriter::addToJSONRow(const std::string& columnName, rapidjson::Value& jsonValue) {
  jsonRow_.AddMember(toJSONString(columnName), jsonValue, jsonPayload_.GetAllocator());
}

rapidjson::Value JSONSQLWriter::toJSONString(const std::string& s) {
  rapidjson::Value jsonValue;
  jsonValue.SetString(s.c_str(), s.size(), jsonPayload_.GetAllocator());

  return jsonValue;
}

std::string JSONSQLWriter::toString() {
  rapidjson::StringBuffer buffer;
  rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);
  jsonPayload_.Accept(writer);

  std::stringstream outputStream;
  outputStream << buffer.GetString();

  jsonPayload_ = rapidjson::Document(rapidjson::kArrayType);

  return outputStream.str();
}

} /* namespace sql */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

