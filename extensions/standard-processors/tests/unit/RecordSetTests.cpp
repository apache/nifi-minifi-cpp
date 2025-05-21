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
 * limitations under the License.c
 */

#include <numbers>
#include <variant>
#include <catch2/generators/catch_generators.hpp>

#include "unit/Catch.h"
#include "unit/TestBase.h"
#include "unit/TestRecord.h"
#include "utils/TimeUtil.h"

using namespace std::literals::chrono_literals;

namespace org::apache::nifi::minifi::standard::test {

TEST_CASE("Test JSON serialization of a RecordField") {
  {
    minifi::core::RecordField field{1};
    rapidjson::Document doc;
    auto value = field.toJson(doc.GetAllocator());
    CHECK(value.GetUint64() == 1);
  }
  {
    minifi::core::RecordField field{-1};
    rapidjson::Document doc;
    auto value = field.toJson(doc.GetAllocator());
    CHECK(value.GetInt64() == -1);
  }
  {
    minifi::core::RecordField field{false};
    rapidjson::Document doc;
    auto value = field.toJson(doc.GetAllocator());
    CHECK(value.GetBool() == false);
  }
  {
    minifi::core::RecordField field{'a'};
    rapidjson::Document doc;
    auto value = field.toJson(doc.GetAllocator());
    CHECK(value.GetString() == std::string("a"));
  }
  {
    minifi::core::RecordField field{1.2};
    rapidjson::Document doc;
    auto value = field.toJson(doc.GetAllocator());
    CHECK(value.GetDouble() == 1.2);
  }
  {
    minifi::core::RecordField field{std::string("hello")};
    rapidjson::Document doc;
    auto value = field.toJson(doc.GetAllocator());
    CHECK(value.GetString() == std::string("hello"));
  }
  {
    std::chrono::time_point test_time = utils::timeutils::parseDateTimeStr("2021-09-01T12:34:56Z").value();
    minifi::core::RecordField field{test_time};
    rapidjson::Document doc;
    auto value = field.toJson(doc.GetAllocator());
    CHECK(value.GetString() == std::string("2021-09-01T12:34:56Z"));
  }
  {
    minifi::core::RecordObject obj;
    obj.emplace("key1", minifi::core::RecordField(1));
    obj.emplace("key2", core::RecordField(std::string("hello")));
    minifi::core::RecordField field{std::move(obj)};
    rapidjson::Document doc;
    auto value = field.toJson(doc.GetAllocator());
    CHECK(value["key1"].GetUint64() == 1);
    CHECK(value["key2"].GetString() == std::string("hello"));
  }

  {
    minifi::core::RecordField field1{-1};
    minifi::core::RecordField field2{true};
    std::vector<minifi::core::RecordField> arr;
    arr.push_back(std::move(field1));
    arr.push_back(std::move(field2));
    minifi::core::RecordField array_field{std::move(arr)};
    rapidjson::Document doc;
    auto value = array_field.toJson(doc.GetAllocator());
    CHECK(value.GetArray()[0].GetInt64() == -1);
    CHECK(value.GetArray()[1].GetBool() == true);
  }
}

TEST_CASE("Test JSON serialization of a Record") {
  minifi::core::Record record;
  record.emplace("key1", minifi::core::RecordField{1});
  record.emplace("key2", minifi::core::RecordField{std::string("hello")});
  record.emplace("key3", minifi::core::RecordField{true});
  record.emplace("key4", minifi::core::RecordField{1.2});
  std::chrono::time_point test_time = utils::timeutils::parseDateTimeStr("2021-09-01T12:34:56Z").value();
  record.emplace("key5", minifi::core::RecordField{test_time});

  minifi::core::RecordField field1{-1};
  minifi::core::RecordField field2{true};
  std::vector<minifi::core::RecordField> arr;
  arr.push_back(std::move(field1));
  arr.push_back(std::move(field2));
  record.emplace("key6", minifi::core::RecordField{std::move(arr)});

  minifi::core::RecordObject subobj;
  subobj.emplace("subkey3", core::RecordField(1));
  subobj.emplace("subkey4", core::RecordField(std::string("subhello")));
  minifi::core::RecordObject obj;
  obj.emplace("subkey1", core::RecordField(-2));
  obj.emplace("subkey2", core::RecordField(std::move(subobj)));
  record.emplace("key7", minifi::core::RecordField{std::move(obj)});

  rapidjson::Document doc = record.toJson();
  CHECK(doc["key1"].GetUint64() == 1);
  CHECK(doc["key2"].GetString() == std::string("hello"));
  CHECK(doc["key3"].GetBool() == true);
  CHECK(doc["key4"].GetDouble() == 1.2);
  CHECK(doc["key5"].GetString() == std::string("2021-09-01T12:34:56Z"));
  CHECK(doc["key6"].GetArray()[0] == -1);
  CHECK(doc["key6"].GetArray()[1] == true);
  CHECK(doc["key7"]["subkey1"].GetInt64() == -2);
  CHECK(doc["key7"]["subkey2"]["subkey3"].GetUint64() == 1);
  CHECK(doc["key7"]["subkey2"]["subkey4"].GetString() == std::string("subhello"));
}

TEST_CASE("Test Record deserialization from JSON") {
  std::string json_str = R"(
{
  "number": 1,
  "string": "hello",
  "bool": false,
  "double": 1.2,
  "array": [1.1, false],
  "time_point": "2021-09-01T12:34:56Z",
  "obj": {
    "number2": 2,
    "message": "mymessage"
  }
}
  )";
  rapidjson::Document doc;
  doc.Parse<0>(json_str);
  auto record = minifi::core::Record::fromJson(doc);
  CHECK(record.at("number") == minifi::core::RecordField{1});
  CHECK(record.at("string") == minifi::core::RecordField{std::string("hello")});
  CHECK(record.at("bool") == minifi::core::RecordField{false});
  CHECK(record.at("double") == minifi::core::RecordField{1.2});
  std::chrono::time_point test_time = utils::timeutils::parseDateTimeStr("2021-09-01T12:34:56Z").value();
  CHECK(record.at("time_point") == minifi::core::RecordField{test_time});

  minifi::core::RecordObject subobj;
  subobj.emplace("number2", core::RecordField(2));
  subobj.emplace("message", core::RecordField(std::string("mymessage")));
  minifi::core::RecordField obj_field{std::move(subobj)};
  CHECK(record.at("obj") == obj_field);

  minifi::core::RecordField field1{1.1};
  minifi::core::RecordField field2{false};
  std::vector<minifi::core::RecordField> arr;
  arr.push_back(std::move(field1));
  arr.push_back(std::move(field2));
  minifi::core::RecordField array_field{std::move(arr)};
  CHECK(record.at("array") == array_field);
}

}  // namespace org::apache::nifi::minifi::standard::test
