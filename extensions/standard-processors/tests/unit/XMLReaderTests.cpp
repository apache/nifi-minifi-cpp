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
#include <unordered_map>

#include "catch2/generators/catch_generators.hpp"
#include "catch2/catch_approx.hpp"
#include "controllers/XMLReader.h"
#include "unit/Catch.h"
#include "unit/TestBase.h"
#include "unit/ControllerServiceUtils.h"

namespace org::apache::nifi::minifi::standard::test {

class XMLReaderTestFixture {
 public:
  XMLReaderTestFixture() : xml_reader_(minifi::test::utils::make_controller_service<XMLReader>("XMLReader")) {
    LogTestController::getInstance().clear();
    LogTestController::getInstance().setTrace<XMLReader>();
  }

  auto readRecordsFromXml(const std::string& xml_input, const std::unordered_map<std::string_view, std::string_view>& properties = {}) {
    initializeTestObject(xml_input, properties);
    return xml_reader_->getImplementation<XMLReader>()->read(buffer_stream_);
  }

 private:
  void initializeTestObject(const std::string& xml_input, const std::unordered_map<std::string_view, std::string_view>& properties = {}) {
    xml_reader_->initialize();
    for (const auto& [key, value] : properties) {
      REQUIRE(xml_reader_->setProperty(key, std::string{value}));
    }
    xml_reader_->onEnable();
    buffer_stream_.write(reinterpret_cast<const uint8_t*>(xml_input.data()), xml_input.size());
  }

  std::unique_ptr<core::controller::ControllerService> xml_reader_;
  io::BufferStream buffer_stream_;
};

TEST_CASE_METHOD(XMLReaderTestFixture, "Invalid XML input or empty input results in error", "[XMLReader]") {
  const std::string xml_input = GENERATE("", "<invalid_xml>");
  auto record_set = readRecordsFromXml(xml_input);
  REQUIRE_FALSE(record_set);
  REQUIRE(LogTestController::getInstance().contains("Failed to parse XML content: " + xml_input));
}

TEST_CASE_METHOD(XMLReaderTestFixture, "XML with only root node results in empty record set", "[XMLReader]") {
  auto record_set = readRecordsFromXml("<root></root>");
  REQUIRE(record_set);
  REQUIRE(record_set->empty());
  REQUIRE(LogTestController::getInstance().contains("XML content does not contain any records: <root></root>"));
}

TEST_CASE_METHOD(XMLReaderTestFixture, "XML contains a single data node results in a single record with default content field name key", "[XMLReader]") {
  auto record_set = readRecordsFromXml("<root>text</root>");
  REQUIRE(record_set->size() == 1);
  auto& record = record_set->at(0);
  CHECK(std::get<std::string>(record.at("value").value_) == "text");
}

TEST_CASE_METHOD(XMLReaderTestFixture, "XML with one empty node", "[XMLReader]") {
  auto record_set = readRecordsFromXml("<root><node></node></root>");
  REQUIRE(record_set);
  REQUIRE(record_set->size() == 1);
  auto& record = record_set->at(0);
  CHECK(std::get<std::string>(record.at("node").value_).empty());
}

TEST_CASE_METHOD(XMLReaderTestFixture, "XML with a single string child node results in a single record", "[XMLReader]") {
  auto record_set = readRecordsFromXml("<root><child>text</child></root>");
  REQUIRE(record_set);
  REQUIRE(record_set->size() == 1);
  auto& record = record_set->at(0);
  CHECK(std::get<std::string>(record.at("child").value_) == "text");
}

TEST_CASE_METHOD(XMLReaderTestFixture, "XML with several child nodes with different types result in a single record", "[XMLReader]") {
  const std::string xml_input = "<root><string>text</string><number>42</number><signed>-23</signed><boolean>true</boolean><double>3.14</double><timestamp>2023-03-15T12:34:56Z</timestamp></root>";
  auto record_set = readRecordsFromXml(xml_input);
  REQUIRE(record_set);
  REQUIRE(record_set->size() == 1);
  auto& record = record_set->at(0);
  CHECK(std::get<std::string>(record.at("string").value_) == "text");
  CHECK(std::get<uint64_t>(record.at("number").value_) == 42);
  CHECK(std::get<int64_t>(record.at("signed").value_) == -23);
  CHECK(std::get<bool>(record.at("boolean").value_) == true);
  CHECK(std::get<double>(record.at("double").value_) == Catch::Approx(3.14));
  auto timestamp = std::get<std::chrono::system_clock::time_point>(record.at("timestamp").value_);
  auto expected_time = utils::timeutils::parseRfc3339("2023-03-15T12:34:56Z");
  REQUIRE(expected_time);
  CHECK(timestamp == *expected_time);
}

TEST_CASE_METHOD(XMLReaderTestFixture, "XML with multiple subnodes result in a single record with record object", "[XMLReader]") {
  const std::string xml_input = "<root><node><subnode1>text1</subnode1><subnode2><subsub1>text2</subsub1><subsub2>text3</subsub2></subnode2></node></root>";
  auto record_set = readRecordsFromXml(xml_input);
  REQUIRE(record_set);
  REQUIRE(record_set->size() == 1);
  auto& record = record_set->at(0);
  auto record_object = std::get<core::RecordObject>(record.at("node").value_);
  REQUIRE(record_object.size() == 2);
  CHECK(std::get<std::string>(record_object.at("subnode1").value_) == "text1");
  CHECK(std::get<std::string>(std::get<core::RecordObject>(record_object.at("subnode2").value_).at("subsub1").value_) == "text2");
  CHECK(std::get<std::string>(std::get<core::RecordObject>(record_object.at("subnode2").value_).at("subsub2").value_) == "text3");
}

TEST_CASE_METHOD(XMLReaderTestFixture, "XML with nodes and text data is parsed correctly", "[XMLReader]") {
  const std::string xml_input = "<root>outtext1<node>nodetext<subnode>subtext</subnode></node>outtext2</root>";
  auto record_set = readRecordsFromXml(xml_input);
  REQUIRE(record_set);
  REQUIRE(record_set->size() == 1);
  auto& record = record_set->at(0);
  CHECK(std::get<std::string>(std::get<core::RecordObject>(record.at("node").value_).at("subnode").value_) == "subtext");
  CHECK(std::get<std::string>(std::get<core::RecordObject>(record.at("node").value_).at("value").value_) == "nodetext");
  CHECK(std::get<std::string>(record.at("value").value_) == "outtext1outtext2");
}

TEST_CASE_METHOD(XMLReaderTestFixture, "XML with same nodes are converted to arrays", "[XMLReader]") {
  const std::string xml_input = "<root><array><item>value1</item><item>value2</item></array></root>";
  auto record_set = readRecordsFromXml(xml_input);
  REQUIRE(record_set);
  REQUIRE(record_set->size() == 1);
  auto& record = record_set->at(0);
  auto& array_field = std::get<core::RecordObject>(record.at("array").value_);
  REQUIRE(array_field.size() == 1);
  auto& item_array = std::get<core::RecordArray>(array_field.at("item").value_);
  REQUIRE(item_array.size() == 2);
  CHECK(std::get<std::string>(item_array[0].value_) == "value1");
  CHECK(std::get<std::string>(item_array[1].value_) == "value2");
}

TEST_CASE_METHOD(XMLReaderTestFixture, "XML nodes with default value tag are ignored if text data is present", "[XMLReader]") {
  const std::string xml_input = "<root>s1<value>s2</value><value>s3</value></root>";
  auto record_set = readRecordsFromXml(xml_input);
  REQUIRE(record_set);
  REQUIRE(record_set->size() == 1);
  auto& record = record_set->at(0);
  CHECK(std::get<std::string>(record.at("value").value_) == "s1");
}

TEST_CASE_METHOD(XMLReaderTestFixture, "Specify Field Name for Content property for tagless values", "[XMLReader]") {
  const std::string xml_input = "<root>outtext<node>nodetext</node></root>";
  auto record_set = readRecordsFromXml(xml_input, {{XMLReader::FieldNameForContent.name, "tagvalue"}});
  REQUIRE(record_set);
  REQUIRE(record_set->size() == 1);
  auto& record = record_set->at(0);
  CHECK(std::get<std::string>(record.at("node").value_) == "nodetext");
  CHECK(std::get<std::string>(record.at("tagvalue").value_) == "outtext");
}

TEST_CASE_METHOD(XMLReaderTestFixture, "Parse attributes as record fields if Parse XML Attributes property is set", "[XMLReader]") {
  const std::string xml_input = R"(<root><node attribute="attr_value">nodetext</node></root>)";
  auto record_set = readRecordsFromXml(xml_input, {{XMLReader::ParseXMLAttributes.name, "true"}});
  REQUIRE(record_set);
  REQUIRE(record_set->size() == 1);
  auto& record = record_set->at(0);
  CHECK(std::get<std::string>(std::get<core::RecordObject>(record.at("node").value_).at("attribute").value_) == "attr_value");
  CHECK(std::get<std::string>(std::get<core::RecordObject>(record.at("node").value_).at("value").value_) == "nodetext");
}

TEST_CASE_METHOD(XMLReaderTestFixture, "Parse attributes as in an XML with nested node array", "[XMLReader]") {
  const std::string xml_input = R"(<root><node attribute="attr_value"><subnode subattr="subattr_value">1</subnode>nodetext<subnode>2</subnode></node></root>)";
  auto record_set = readRecordsFromXml(xml_input, {{XMLReader::ParseXMLAttributes.name, "true"}});
  REQUIRE(record_set);
  REQUIRE(record_set->size() == 1);
  auto& record = record_set->at(0);
  auto& node_object = std::get<core::RecordObject>(record.at("node").value_);
  CHECK(node_object.size() == 3);
  CHECK(std::get<std::string>(node_object.at("attribute").value_) == "attr_value");
  CHECK(std::get<std::string>(node_object.at("value").value_) == "nodetext");
  auto& subnodes = std::get<core::RecordArray>(node_object.at("subnode").value_);
  CHECK(subnodes.size() == 2);
  const auto& subnode_object = std::get<core::RecordObject>(subnodes[0].value_);
  CHECK(std::get<std::string>(subnode_object.at("subattr").value_) == "subattr_value");
  CHECK(std::get<uint64_t>(subnode_object.at("value").value_) == 1);
  CHECK(std::get<uint64_t>(subnodes[1].value_) == 2);
}

TEST_CASE_METHOD(XMLReaderTestFixture, "Attributes clashing with the content field name are ignored", "[XMLReader]") {
  const std::string xml_input = R"(<root><node><subnode attr="attr_value" tagvalue="attr_value2">value</subnode></node></root>)";
  auto record_set = readRecordsFromXml(xml_input, {
    {XMLReader::ParseXMLAttributes.name, "true"},
    {XMLReader::FieldNameForContent.name, "tagvalue"}
  });
  REQUIRE(record_set);
  REQUIRE(record_set->size() == 1);
  auto& record = record_set->at(0);
  auto& node_object = std::get<core::RecordObject>(record.at("node").value_);
  auto& a_object = std::get<core::RecordObject>(node_object.at("subnode").value_);
  CHECK(a_object.size() == 2);
  CHECK(std::get<std::string>(a_object.at("attr").value_) == "attr_value");
  CHECK(std::get<std::string>(a_object.at("tagvalue").value_) == "value");
}

TEST_CASE_METHOD(XMLReaderTestFixture, "Nodes clashing with the content field name are ignored", "[XMLReader]") {
  const std::string xml_input = R"(<root><node>value<tagvalue>ignored</tagvalue></node></root>)";
  auto record_set = readRecordsFromXml(xml_input, {{XMLReader::FieldNameForContent.name, "tagvalue"}});
  REQUIRE(record_set);
  REQUIRE(record_set->size() == 1);
  auto& record = record_set->at(0);
  auto& node_object = std::get<core::RecordObject>(record.at("node").value_);
  CHECK(node_object.size() == 1);
  CHECK(std::get<std::string>(node_object.at("tagvalue").value_) == "value");
}

TEST_CASE_METHOD(XMLReaderTestFixture, "Attributes are prefixed with the defined prefix", "[XMLReader]") {
  const std::string xml_input = R"(<root><node><subnode mykey="myattrval" fieldname="myattrval2">value</subnode></node></root>)";
  auto record_set = readRecordsFromXml(xml_input, {
    {XMLReader::ParseXMLAttributes.name, "true"},
    {XMLReader::FieldNameForContent.name, "fieldname"},
    {XMLReader::AttributePrefix.name, "attr_"}
  });
  REQUIRE(record_set);
  REQUIRE(record_set->size() == 1);
  auto& record = record_set->at(0);
  auto& node_object = std::get<core::RecordObject>(record.at("node").value_);
  auto& a_object = std::get<core::RecordObject>(node_object.at("subnode").value_);
  CHECK(a_object.size() == 3);
  CHECK(std::get<std::string>(a_object.at("attr_mykey").value_) == "myattrval");
  CHECK(std::get<std::string>(a_object.at("attr_fieldname").value_) == "myattrval2");
  CHECK(std::get<std::string>(a_object.at("fieldname").value_) == "value");
}

TEST_CASE_METHOD(XMLReaderTestFixture, "Read multiple records from XML", "[XMLReader]") {
  const std::string xml_input = "<root><node><message><from>Tony</from><to>Bob</to><body>Hello</body></message></node><node>Hi!</node></root>";
  auto record_set = readRecordsFromXml(xml_input, {{XMLReader::ExpectRecordsAsArray.name, "true"}});
  REQUIRE(record_set);
  REQUIRE(record_set->size() == 2);
  auto& record1 = record_set->at(0);
  auto& message_record = std::get<core::RecordObject>(record1.at("message").value_);
  CHECK(message_record.size() == 3);
  CHECK(std::get<std::string>(message_record.at("from").value_) == "Tony");
  CHECK(std::get<std::string>(message_record.at("to").value_) == "Bob");
  CHECK(std::get<std::string>(message_record.at("body").value_) == "Hello");
  auto& record2 = record_set->at(1);
  CHECK(std::get<std::string>(record2.at("value").value_) == "Hi!");
}

}  // namespace org::apache::nifi::minifi::standard::test
