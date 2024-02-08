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

#include "Catch.h"
#include "RecordSetTesters.h"
#include "TestBase.h"
#include "TestRecord.h"
#include "controllers/JsonRecordSetReader.h"
#include "controllers/JsonRecordSetWriter.h"
#include "core/Record.h"

namespace org::apache::nifi::minifi::standard::test {

constexpr std::string_view record_per_line_str = R"({"baz":3.14,"qux":[true,false,true],"is_test":true,"bar":123,"quux":{"Aprikose":"apricot","Birne":"pear","Apfel":"apple"},"foo":"asd","when":"2012-07-01T09:53:00Z"}
{"baz":3.141592653589793,"qux":[false,false,true],"is_test":true,"bar":98402134,"quux":{"Aprikose":"abricot","Birne":"poire","Apfel":"pomme"},"foo":"Lorem ipsum dolor sit amet, consectetur adipiscing elit.","when":"2022-11-01T19:52:11Z"}
)";
constexpr std::string_view array_compressed_str = R"([{"baz":3.14,"qux":[true,false,true],"is_test":true,"bar":123,"quux":{"Aprikose":"apricot","Birne":"pear","Apfel":"apple"},"foo":"asd","when":"2012-07-01T09:53:00Z"},{"baz":3.141592653589793,"qux":[false,false,true],"is_test":true,"bar":98402134,"quux":{"Aprikose":"abricot","Birne":"poire","Apfel":"pomme"},"foo":"Lorem ipsum dolor sit amet, consectetur adipiscing elit.","when":"2022-11-01T19:52:11Z"}])";
constexpr std::string_view array_pretty_str = R"([
    {
        "baz": 3.14,
        "qux": [
            true,
            false,
            true
        ],
        "is_test": true,
        "bar": 123,
        "quux": {
            "Aprikose": "apricot",
            "Birne": "pear",
            "Apfel": "apple"
        },
        "foo": "asd",
        "when": "2012-07-01T09:53:00Z"
    },
    {
        "baz": 3.141592653589793,
        "qux": [
            false,
            false,
            true
        ],
        "is_test": true,
        "bar": 98402134,
        "quux": {
            "Aprikose": "abricot",
            "Birne": "poire",
            "Apfel": "pomme"
        },
        "foo": "Lorem ipsum dolor sit amet, consectetur adipiscing elit.",
        "when": "2022-11-01T19:52:11Z"
    }
])";

bool test_json_equality(const std::string_view expected_str, const std::string_view actual_str) {
  rapidjson::Document expected;
  expected.Parse(expected_str.data());
  rapidjson::Document actual;
  actual.Parse(actual_str.data());
  return actual == expected;
}

TEST_CASE("JsonRecordSetWriter test Record Per Line") {
  core::RecordSet record_set;
  record_set.push_back(core::test::createSampleRecord());
  record_set.push_back(core::test::createSampleRecord2());

  JsonRecordSetWriter json_record_set_writer;
  json_record_set_writer.initialize();
  CHECK(json_record_set_writer.setProperty(JsonRecordSetWriter::OutputGrouping, "OneLinePerObject"));
  json_record_set_writer.onEnable();
  CHECK(core::test::testRecordWriter(json_record_set_writer, record_set, [](auto serialized_record_set) -> bool {
    return test_json_equality(record_per_line_str, serialized_record_set);
  }));
}

TEST_CASE("JsonRecordSetWriter test array") {
  core::RecordSet record_set;
  record_set.push_back(core::test::createSampleRecord());
  record_set.push_back(core::test::createSampleRecord2());

  JsonRecordSetWriter json_record_set_writer;
  CHECK(core::test::testRecordWriter(json_record_set_writer, record_set, [](auto serialized_record_set) -> bool {
    return test_json_equality(array_compressed_str, serialized_record_set);
  }));
}

TEST_CASE("JsonRecordSetWriter test pretty array") {
  core::RecordSet record_set;
  record_set.push_back(core::test::createSampleRecord());
  record_set.push_back(core::test::createSampleRecord2());

  JsonRecordSetWriter json_record_set_writer;
  json_record_set_writer.initialize();
  CHECK(json_record_set_writer.setProperty(JsonRecordSetWriter::PrettyPrint, "true"));
  json_record_set_writer.onEnable();
  CHECK(core::test::testRecordWriter(json_record_set_writer, record_set, [](auto serialized_record_set) -> bool {
    return test_json_equality(array_pretty_str, serialized_record_set);
  }));
}

TEST_CASE("JsonRecordSetReader per line") {
  core::RecordSet expected_record_set;
  expected_record_set.push_back(core::test::createSampleRecord(true));
  expected_record_set.push_back(core::test::createSampleRecord2(true));

  JsonRecordSetReader json_record_set_reader;
  CHECK(core::test::testRecordReader(json_record_set_reader, record_per_line_str, expected_record_set));
}

TEST_CASE("JsonRecordSetReader compressed array") {
  core::RecordSet expected_record_set;
  expected_record_set.push_back(core::test::createSampleRecord(true));
  expected_record_set.push_back(core::test::createSampleRecord2(true));

  JsonRecordSetReader json_record_set_reader;
  CHECK(core::test::testRecordReader(json_record_set_reader, array_compressed_str, expected_record_set));
}

TEST_CASE("JsonRecordSetReader pretty array") {
  core::RecordSet expected_record_set;
  expected_record_set.push_back(core::test::createSampleRecord(true));
  expected_record_set.push_back(core::test::createSampleRecord2(true));

  JsonRecordSetReader json_record_set_reader;
  CHECK(core::test::testRecordReader(json_record_set_reader, array_pretty_str, expected_record_set));
}
}  // namespace org::apache::nifi::minifi::standard::test
