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
#include <unordered_map>
#include <string_view>

#include "pugixml.hpp"
#include "unit/Catch.h"
#include "unit/TestBase.h"
#include "../controllers/XMLRecordSetWriter.h"
#include "io/BufferStream.h"
#include "core/ProcessSession.h"
#include "catch2/generators/catch_generators.hpp"
#include "utils/StringUtils.h"
#include "unit/ControllerServiceUtils.h"

namespace org::apache::nifi::minifi::test {

class XMLRecordSetWriterTestFixture {
 public:
  const core::Relationship Success{"success", "everything is fine"};

  XMLRecordSetWriterTestFixture() : xml_record_set_writer_(minifi::test::utils::make_controller_service<standard::XMLRecordSetWriter>("XMLRecordSetWriter")) {
    test_plan_ = test_controller_.createPlan();
    dummy_processor_ = test_plan_->addProcessor("DummyProcessor", "dummyProcessor");
    context_ = [this] {
      test_plan_->runNextProcessor();
      return test_plan_->getCurrentContext();
    }();
    process_session_ = std::make_unique<core::ProcessSessionImpl>(context_);
  }

  std::string writeRecordsAsXml(const core::RecordSet& record_set, const std::unordered_map<std::string_view, std::string_view>& properties) {
    xml_record_set_writer_->initialize();
    for (const auto& [key, value] : properties) {
      REQUIRE(xml_record_set_writer_->setProperty(key, std::string{value}));
    }
    xml_record_set_writer_->onEnable();

    auto flow_file = process_session_->create();
    xml_record_set_writer_->getImplementation<standard::XMLRecordSetWriter>()->write(record_set, flow_file, *process_session_);
    transferAndCommit(flow_file);
    std::string xml_content;
    process_session_->read(*flow_file, [&xml_content](const std::shared_ptr<io::InputStream>& input_stream) {
      std::vector<std::byte> buffer(input_stream->size());
      input_stream->read(buffer);
      xml_content = std::string(reinterpret_cast<const char*>(buffer.data()), buffer.size());
      return gsl::narrow<int64_t>(input_stream->size());
    });
    return xml_content;
  }

  static void verifyValuesUnderNode(const std::string& xml_content, const std::string& node_path, const std::unordered_map<std::string, std::string>& expected_values) {
    gsl_Expects(!expected_values.empty());
    pugi::xml_document doc;
    REQUIRE(doc.load_string(xml_content.c_str()));

    pugi::xml_node node = doc.document_element();
    auto node_names = minifi::utils::string::splitAndTrimRemovingEmpty(node_path, "/");
    gsl_Assert(!node_names.empty());
    REQUIRE(std::string(node.name()) == node_names[0]);
    for (size_t i = 1; i < node_names.size(); ++i) {
      node = node.child(node_names[i].c_str());
      REQUIRE(node);
    }

    for (const auto& [field_name, expected_value] : expected_values) {
      verifyXmlValue(node, field_name, expected_value);
    }
  }

  static void verifyArrayValuesUnderNode(const std::string& xml_content, const std::string& node_path, const std::unordered_set<std::string>& expected_values) {
    gsl_Expects(!expected_values.empty());
    pugi::xml_document doc;
    REQUIRE(doc.load_string(xml_content.c_str()));

    pugi::xml_node node = doc.document_element();
    auto node_names = minifi::utils::string::splitAndTrimRemovingEmpty(node_path, "/");
    gsl_Assert(!node_names.empty());
    REQUIRE(std::string(node.name()) == node_names[0]);
    for (size_t i = 1; i < node_names.size() - 1; ++i) {
      node = node.child(node_names[i].c_str());
      REQUIRE(node);
    }

    size_t count = 0;
    for (const auto& child : node.children(node_names.back().c_str())) {
      ++count;
      REQUIRE(child);
      CHECK(expected_values.contains(std::string{child.child_value()}));
    }

    REQUIRE(count == expected_values.size());
  }

  static void verifyXmlValue(const pugi::xml_node& node, const std::string& field_name, const std::string& expected_value) {
    auto field_node = node.child(field_name.c_str());
    REQUIRE(field_node);
    std::string child_value = field_node.child_value();
    CHECK(child_value == expected_value);
  };

 private:
  void transferAndCommit(const std::shared_ptr<core::FlowFile>& flow_file) {
    process_session_->transfer(flow_file, Success);
    process_session_->commit();
  }

  TestController test_controller_;

  std::shared_ptr<TestPlan> test_plan_;
  core::Processor* dummy_processor_;
  std::shared_ptr<core::ProcessContext> context_;
  std::unique_ptr<core::ProcessSession> process_session_;
  std::unique_ptr<core::controller::ControllerService> xml_record_set_writer_;
};

TEST_CASE_METHOD(XMLRecordSetWriterTestFixture, "If wrap elements of arrays is set then Array Tag Name property must be set", "[XMLRecordSetWriter]") {
  auto xml_record_set_writer = minifi::test::utils::make_controller_service<standard::XMLRecordSetWriter>("XMLRecordSetWriter");
  xml_record_set_writer->initialize();
  REQUIRE(xml_record_set_writer->setProperty(standard::XMLRecordSetWriter::NameOfRecordTag.name, "record"));
  REQUIRE(xml_record_set_writer->setProperty(standard::XMLRecordSetWriter::NameOfRootTag.name, "root"));
  std::string wrap_element_option = GENERATE("Use Property as Wrapper", "Use Property for Elements");
  REQUIRE(xml_record_set_writer->setProperty(standard::XMLRecordSetWriter::WrapElementsOfArrays.name, wrap_element_option));
  REQUIRE_THROWS_WITH(xml_record_set_writer->onEnable(),
    "Process Schedule Operation: Array Tag Name property must be set when Wrap Elements of Arrays is set to Use Property as Wrapper or Use Property for Elements");
}

TEST_CASE_METHOD(XMLRecordSetWriterTestFixture, "Name of Record Tag must be set", "[XMLRecordSetWriter]") {
  auto xml_record_set_writer = minifi::test::utils::make_controller_service<standard::XMLRecordSetWriter>("XMLRecordSetWriter");
  xml_record_set_writer->initialize();
  REQUIRE_THROWS_WITH(xml_record_set_writer->onEnable(), "Process Schedule Operation: Name of Record Tag property must be set");
}

TEST_CASE_METHOD(XMLRecordSetWriterTestFixture, "Name of Root Tag must be set", "[XMLRecordSetWriter]") {
  auto xml_record_set_writer = minifi::test::utils::make_controller_service<standard::XMLRecordSetWriter>("XMLRecordSetWriter");
  xml_record_set_writer->initialize();
  REQUIRE(xml_record_set_writer->setProperty(standard::XMLRecordSetWriter::NameOfRecordTag.name, "record"));
  REQUIRE_THROWS_WITH(xml_record_set_writer->onEnable(), "Process Schedule Operation: Name of Root Tag property must be set");
}

TEST_CASE_METHOD(XMLRecordSetWriterTestFixture, "Test empty record set", "[XMLRecordSetWriter]") {
  core::RecordSet record_set;

  bool omit_xml_declaration = false;
  std::string expected_xml;
  SECTION("Use XML declaration") {
    expected_xml = R"(<?xml version="1.0"?><root/>)";
  }

  SECTION("Omit XML declaration") {
    omit_xml_declaration = true;
    expected_xml = R"(<root/>)";
  }

  auto xml_content = writeRecordsAsXml(record_set, {
    {standard::XMLRecordSetWriter::OmitXMLDeclaration.name, omit_xml_declaration ? "true" : "false"},
    {standard::XMLRecordSetWriter::NameOfRecordTag.name, "record"},
    {standard::XMLRecordSetWriter::NameOfRootTag.name, "root"}
  });
  REQUIRE(xml_content == expected_xml);
}

TEST_CASE_METHOD(XMLRecordSetWriterTestFixture, "Test single record with primitive values", "[XMLRecordSetWriter]") {
  core::RecordSet record_set;
  core::RecordObject record_object;
  record_object.emplace("string_field", core::RecordField(std::string("value1")));
  record_object.emplace("uint_field", core::RecordField(static_cast<uint64_t>(42)));
  record_object.emplace("double_field", core::RecordField(2.3));
  record_object.emplace("bool_field", core::RecordField(true));
  record_object.emplace("time_point_field", core::RecordField(std::chrono::system_clock::time_point(std::chrono::sys_days(std::chrono::year(2025)/1/1))));
  record_set.emplace_back(std::move(record_object));

  auto xml_content = writeRecordsAsXml(record_set, {
    {standard::XMLRecordSetWriter::NameOfRecordTag.name, "record"},
    {standard::XMLRecordSetWriter::NameOfRootTag.name, "root"}
  });

  verifyValuesUnderNode(xml_content, "root/record", {
    {"string_field", "value1"},
    {"uint_field", "42"},
    {"double_field", "2.3"},
    {"bool_field", "true"},
    {"time_point_field", "2025-01-01T00:00:00Z"}
  });
}

TEST_CASE_METHOD(XMLRecordSetWriterTestFixture, "Test single record with object value", "[XMLRecordSetWriter]") {
  core::RecordSet record_set;
  core::RecordObject record_object;
  record_object.emplace("string_field", core::RecordField(std::string("value1")));
  core::RecordObject inner_object;
  inner_object.emplace("inner_field", core::RecordField(std::string("inner_value")));
  record_object.emplace("inner_object", core::RecordField(std::move(inner_object)));
  record_set.emplace_back(std::move(record_object));

  auto xml_content = writeRecordsAsXml(record_set, {
    {standard::XMLRecordSetWriter::NameOfRecordTag.name, "record"},
    {standard::XMLRecordSetWriter::NameOfRootTag.name, "root"}
  });

  verifyValuesUnderNode(xml_content, "root/record", {
    {"string_field", "value1"}
  });
  verifyValuesUnderNode(xml_content, "root/record/inner_object", {
    {"inner_field", "inner_value"}
  });
}

TEST_CASE_METHOD(XMLRecordSetWriterTestFixture, "Test single record with object array", "[XMLRecordSetWriter]") {
  core::RecordSet record_set;
  core::RecordObject record_object;
  record_object.emplace("string_field", core::RecordField(std::string("value1")));
  core::RecordObject inner_object;
  inner_object.emplace("inner_field", core::RecordField(core::RecordArray{
    core::RecordField(std::string("inner_value1")),
    core::RecordField(std::string("inner_value2"))
  }));
  record_object.emplace("inner_object", core::RecordField(std::move(inner_object)));
  record_set.emplace_back(std::move(record_object));

  auto xml_content = writeRecordsAsXml(record_set, {
    {standard::XMLRecordSetWriter::NameOfRecordTag.name, "record"},
    {standard::XMLRecordSetWriter::NameOfRootTag.name, "root"}
  });

  verifyValuesUnderNode(xml_content, "root/record", {
    {"string_field", "value1"}
  });

  verifyArrayValuesUnderNode(xml_content, "root/record/inner_object/inner_field", {"inner_value1", "inner_value2"});
}

TEST_CASE_METHOD(XMLRecordSetWriterTestFixture, "Test single record with array tag name used as wrapper node", "[XMLRecordSetWriter]") {
  core::RecordSet record_set;
  core::RecordObject record_object;
  record_object.emplace("array_field", core::RecordField(core::RecordArray{
    core::RecordField(std::string("inner_value1")),
    core::RecordField(std::string("inner_value2"))
  }));
  record_set.emplace_back(std::move(record_object));

  auto xml_content = writeRecordsAsXml(record_set, {
    {standard::XMLRecordSetWriter::NameOfRecordTag.name, "record"},
    {standard::XMLRecordSetWriter::NameOfRootTag.name, "root"},
    {standard::XMLRecordSetWriter::WrapElementsOfArrays.name, "Use Property as Wrapper"},
    {standard::XMLRecordSetWriter::ArrayTagName.name, "array"}
  });

  verifyArrayValuesUnderNode(xml_content, "root/record/array/array_field", {"inner_value1", "inner_value2"});
}

TEST_CASE_METHOD(XMLRecordSetWriterTestFixture, "Test single record with array tag name used as element node", "[XMLRecordSetWriter]") {
  core::RecordSet record_set;
  core::RecordObject record_object;
  record_object.emplace("array_field", core::RecordField(core::RecordArray{
    core::RecordField(std::string("inner_value1")),
    core::RecordField(std::string("inner_value2"))
  }));
  record_set.emplace_back(std::move(record_object));

  auto xml_content = writeRecordsAsXml(record_set, {
    {standard::XMLRecordSetWriter::NameOfRecordTag.name, "record"},
    {standard::XMLRecordSetWriter::NameOfRootTag.name, "root"},
    {standard::XMLRecordSetWriter::WrapElementsOfArrays.name, "Use Property for Elements"},
    {standard::XMLRecordSetWriter::ArrayTagName.name, "element_name"}
  });

  verifyArrayValuesUnderNode(xml_content, "root/record/array_field/element_name", {"inner_value1", "inner_value2"});
}

TEST_CASE_METHOD(XMLRecordSetWriterTestFixture, "Test multiple records wrapped", "[XMLRecordSetWriter]") {
  core::RecordSet record_set;
  core::RecordObject record_object_1;
  record_object_1.emplace("string_field", core::RecordField(std::string("value1")));
  record_object_1.emplace("uint_field", core::RecordField(static_cast<uint64_t>(42)));
  record_set.emplace_back(std::move(record_object_1));
  core::RecordObject record_object_2;
  record_object_2.emplace("string_field", core::RecordField(std::string("value1")));
  record_object_2.emplace("uint_field", core::RecordField(static_cast<uint64_t>(42)));
  record_set.emplace_back(std::move(record_object_2));

  auto xml_content = writeRecordsAsXml(record_set, {
    {standard::XMLRecordSetWriter::NameOfRecordTag.name, "record"},
    {standard::XMLRecordSetWriter::NameOfRootTag.name, "root"}
  });

  pugi::xml_document doc;
  REQUIRE(doc.load_string(xml_content.c_str()));
  auto root_node = doc.child("root");
  REQUIRE(root_node);

  size_t count = 0;
  for (const auto& record_node : root_node.children("record")) {
    REQUIRE(record_node);
    verifyXmlValue(record_node, "string_field", "value1");
    verifyXmlValue(record_node, "uint_field", "42");
    ++count;
  }

  REQUIRE(count == 2);
}

TEST_CASE_METHOD(XMLRecordSetWriterTestFixture, "Test pretty print XML", "[XMLRecordSetWriter]") {
  core::RecordSet record_set;
  core::RecordObject record_object;
  record_object.emplace("bool_field", core::RecordField(true));
  record_set.emplace_back(std::move(record_object));

  auto xml_content = writeRecordsAsXml(record_set, {
    {standard::XMLRecordSetWriter::NameOfRecordTag.name, "record"},
    {standard::XMLRecordSetWriter::NameOfRootTag.name, "root"},
    {standard::XMLRecordSetWriter::PrettyPrintXML.name, "true"}
  });

  REQUIRE(xml_content ==
R"(<?xml version="1.0"?>
<root>
  <record>
    <bool_field>true</bool_field>
  </record>
</root>
)");
}

}  // namespace org::apache::nifi::minifi::test
