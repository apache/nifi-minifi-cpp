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

#include <map>
#include <string>

#include "TestBase.h"
#include "Catch.h"
#include "core/Core.h"
#include "utils/OsUtils.h"
#include "wel/MetadataWalker.h"
#include "wel/XMLString.h"
#include "pugixml.hpp"

using METADATA = org::apache::nifi::minifi::wel::METADATA;
using MetadataWalker = org::apache::nifi::minifi::wel::MetadataWalker;
using WindowsEventLogMetadata = org::apache::nifi::minifi::wel::WindowsEventLogMetadata;
using WindowsEventLogMetadataImpl = org::apache::nifi::minifi::wel::WindowsEventLogMetadataImpl;
using XmlString = org::apache::nifi::minifi::wel::XmlString;

namespace {

std::string updateXmlMetadata(const std::string &xml, EVT_HANDLE metadata_ptr, EVT_HANDLE event_ptr, bool update_xml, bool resolve, utils::Regex const* regex = nullptr) {
  WindowsEventLogMetadataImpl metadata{metadata_ptr, event_ptr};
  MetadataWalker walker(metadata, "", update_xml, resolve, regex, &utils::OsUtils::userIdToUsername);

  pugi::xml_document doc;
  pugi::xml_parse_result result = doc.load_string(xml.c_str());

  if (result) {
    doc.traverse(walker);
    XmlString writer;
    doc.print(writer, "", pugi::format_raw);  // no indentation or formatting
    return writer.xml_;
  } else {
    throw std::runtime_error("Could not parse XML document");
  }
}

std::string formatXml(const std::string &xml) {
  pugi::xml_document doc;
  pugi::xml_parse_result result = doc.load_string(xml.c_str());

  if (result) {
    XmlString writer;
    doc.print(writer, "", pugi::format_raw);  // no indentation or formatting
    return writer.xml_;
  }
  return xml;
}

std::string readFile(const std::string &file_name) {
  std::ifstream file{file_name};
  return std::string{std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}

const std::string METADATA_WALKER_TESTS_LOG_NAME = "MetadataWalkerTests";
const short event_type_index = 178;  // NOLINT short comes from WINDOWS API

class FakeWindowsEventLogMetadata : public WindowsEventLogMetadata {
 public:
  [[nodiscard]] std::string getEventData(EVT_FORMAT_MESSAGE_FLAGS flags) const override { return "event_data_for_flag_" + std::to_string(flags); }
  [[nodiscard]] std::string getEventTimestamp() const override { return "event_timestamp"; }
  short getEventTypeIndex() const override { return event_type_index; }  // NOLINT short comes from WINDOWS API
};

}  // namespace

TEST_CASE("MetadataWalker updates the Sid in the XML if both update_xml and resolve are true", "[updateXmlMetadata]") {
  std::string xml = readFile("resources/nobodysid.xml");

  SECTION("No resolution") {
    REQUIRE(updateXmlMetadata(xml, nullptr, nullptr, false, true) == formatXml(xml));
  }

  SECTION("Resolve nobody") {
    std::string nobody = readFile("resources/withsids.xml");
    auto regex = utils::Regex(".*Sid");
    REQUIRE(updateXmlMetadata(xml, nullptr, nullptr, true, true, &regex) == formatXml(nobody));
  }
}

TEST_CASE("MetadataWalker works even when there is no Data block", "[updateXmlMetadata]") {
  std::string xml = readFile("resources/nodata.xml");

  REQUIRE(updateXmlMetadata(xml, nullptr, nullptr, false, true) == formatXml(xml));
}

TEST_CASE("MetadataWalker throws if the input XML is invalid", "[updateXmlMetadata]") {
  std::string xml = readFile("resources/invalidxml.xml");

  REQUIRE_THROWS(updateXmlMetadata(xml, nullptr, nullptr, false, true) == formatXml(xml));
}

TEST_CASE("MetadataWalker will leave a Sid unchanged if it doesn't correspond to a user", "[updateXmlMetadata]") {
  std::string xml = readFile("resources/unknownsid.xml");

  REQUIRE(updateXmlMetadata(xml, nullptr, nullptr, false, true) == formatXml(xml));
  REQUIRE(updateXmlMetadata(xml, nullptr, nullptr, true, true) == formatXml(xml));
  auto regex = utils::Regex(".*Sid");
  REQUIRE(updateXmlMetadata(xml, nullptr, nullptr, true, true, &regex) == formatXml(xml));
}

TEST_CASE("MetadataWalker can replace multiple Sids", "[updateXmlMetadata]") {
  std::string xml = readFile("resources/multiplesids.xml");

  pugi::xml_document doc;
  xml = updateXmlMetadata(xml, nullptr, nullptr, false, true);
  pugi::xml_parse_result result = doc.load_string(xml.c_str());
  REQUIRE(result);

  // we are only testing multiple sid resolutions, not the resolution of other items.
  CHECK(std::string_view("Nobody Everyone Null Authority") == doc.select_node("Event/EventData/Data[@Name='GroupMembership']").node().text().get());
}

namespace {

void extractMappingsTestHelper(const std::string &file_name,
                               bool update_xml,
                               bool resolve,
                               const std::map<std::string, std::string>& expected_identifiers,
                               const std::map<METADATA, std::string>& expected_metadata,
                               const std::map<std::string, std::string>& expected_field_values) {
  std::string input_xml = readFile(file_name);
  REQUIRE(!input_xml.empty());
  pugi::xml_document doc;
  pugi::xml_parse_result result = doc.load_string(input_xml.c_str());
  REQUIRE(result);

  auto regex = utils::Regex(".*Sid");
  MetadataWalker walker(FakeWindowsEventLogMetadata{}, METADATA_WALKER_TESTS_LOG_NAME, update_xml, resolve, &regex, &utils::OsUtils::userIdToUsername);
  doc.traverse(walker);

  CHECK(walker.getIdentifiers() == expected_identifiers);
  CHECK(walker.getFieldValues() == expected_field_values);
  for (const auto &key_value_pair : expected_metadata) {
    CHECK(walker.getMetadata(key_value_pair.first) == key_value_pair.second);
  }
}

}  // namespace

TEST_CASE("MetadataWalker extracts mappings correctly when there is a single Sid and resolve=false", "[for_each]") {
  const std::string file_name = "resources/nobodysid.xml";

  const std::map<std::string, std::string> expected_identifiers{{"S-1-0-0", "S-1-0-0"}};

  using org::apache::nifi::minifi::wel::METADATA;
  const std::map<METADATA, std::string> expected_metadata{
      {METADATA::SOURCE, "Microsoft-Windows-Security-Auditing"},
      {METADATA::TIME_CREATED, "event_timestamp"},
      {METADATA::EVENTID, "4672"},
      {METADATA::EVENT_RECORDID, "2575952"}};

  const std::map<std::string, std::string> expected_field_values{};

  SECTION("update_xml is false") {
    extractMappingsTestHelper(file_name, false, false, expected_identifiers, expected_metadata, expected_field_values);
  }

  SECTION("update_xml is true") {
    extractMappingsTestHelper(file_name, true, false, expected_identifiers, expected_metadata, expected_field_values);
  }
}

TEST_CASE("MetadataWalker extracts mappings correctly when there is a single Sid and resolve=true", "[for_each]") {
  const std::string file_name = "resources/nobodysid.xml";

  const std::map<std::string, std::string> expected_identifiers{{"S-1-0-0", "Nobody"}};

  using org::apache::nifi::minifi::wel::METADATA;
  const std::map<METADATA, std::string> expected_metadata{
      {METADATA::LOG_NAME, "MetadataWalkerTests"},
      {METADATA::SOURCE, "Microsoft-Windows-Security-Auditing"},
      {METADATA::TIME_CREATED, "event_timestamp"},
      {METADATA::EVENTID, "4672"},
      {METADATA::OPCODE, "event_data_for_flag_4"},
      {METADATA::EVENT_RECORDID, "2575952"},
      {METADATA::EVENT_TYPE, "178"},
      {METADATA::TASK_CATEGORY, "event_data_for_flag_3"},
      {METADATA::LEVEL, "event_data_for_flag_2"},
      {METADATA::KEYWORDS, "event_data_for_flag_5"}};

  SECTION("update_xml is false => fields are collected into walker.getFieldValues()") {
    const std::map<std::string, std::string> expected_field_values{
        {"Channel", "event_data_for_flag_6"},
        {"Keywords", "event_data_for_flag_5"},
        {"Level", "event_data_for_flag_2"},
        {"Opcode", "event_data_for_flag_4"},
        {"SubjectUserSid", "Nobody"},
        {"Task", "event_data_for_flag_3"}};

    extractMappingsTestHelper(file_name, false, true, expected_identifiers, expected_metadata, expected_field_values);
  }

  SECTION("update_xml is true => fields are updated in-place in the XML, and walker.getFieldValues() is empty") {
    const std::map<std::string, std::string> expected_field_values{};

    extractMappingsTestHelper(file_name, true, true, expected_identifiers, expected_metadata, expected_field_values);
  }
}

TEST_CASE("MetadataWalker extracts mappings correctly when there are multiple Sids and resolve=false", "[for_each]") {
  const std::string file_name = "resources/multiplesids.xml";

  const std::map<std::string, std::string> expected_identifiers{{"S-1-0-0", "S-1-0-0"}};

  using org::apache::nifi::minifi::wel::METADATA;
  const std::map<METADATA, std::string> expected_metadata{
      {METADATA::SOURCE, "Microsoft-Windows-Security-Auditing"},
      {METADATA::TIME_CREATED, "event_timestamp"},
      {METADATA::EVENTID, "4672"},
      {METADATA::EVENT_RECORDID, "2575952"}};

  const std::map<std::string, std::string> expected_field_values{};

  SECTION("update_xml is false") {
    extractMappingsTestHelper(file_name, false, false, expected_identifiers, expected_metadata, expected_field_values);
  }

  SECTION("update_xml is true") {
    extractMappingsTestHelper(file_name, true, false, expected_identifiers, expected_metadata, expected_field_values);
  }
}

TEST_CASE("MetadataWalker extracts mappings correctly when there are multiple Sids and resolve=true", "[for_each]") {
  const std::string file_name = "resources/multiplesids.xml";

  const std::map<std::string, std::string> expected_identifiers{
      {"%{S-1-0}", "Null Authority"},
      {"%{S-1-0-0}", "Nobody"},
      {"%{S-1-1-0}", "Everyone"},
      {"S-1-0", "Null Authority"},
      {"S-1-0-0", "Nobody"},
      {"S-1-1-0", "Everyone"}};

  using org::apache::nifi::minifi::wel::METADATA;
  const std::map<METADATA, std::string> expected_metadata{
      {METADATA::LOG_NAME, "MetadataWalkerTests"},
      {METADATA::SOURCE, "Microsoft-Windows-Security-Auditing"},
      {METADATA::TIME_CREATED, "event_timestamp"},
      {METADATA::EVENTID, "4672"},
      {METADATA::OPCODE, "event_data_for_flag_4"},
      {METADATA::EVENT_RECORDID, "2575952"},
      {METADATA::EVENT_TYPE, "178"},
      {METADATA::TASK_CATEGORY, "event_data_for_flag_3"},
      {METADATA::LEVEL, "event_data_for_flag_2"},
      {METADATA::KEYWORDS, "event_data_for_flag_5"}};

  SECTION("update_xml is false => fields are collected into walker.getFieldValues()") {
    const std::map<std::string, std::string> expected_field_values{
        {"Channel", "event_data_for_flag_6"},
        {"Keywords", "event_data_for_flag_5"},
        {"Level", "event_data_for_flag_2"},
        {"Opcode", "event_data_for_flag_4"},
        {"SubjectUserSid", "Nobody"},
        {"Task", "event_data_for_flag_3"}};

    extractMappingsTestHelper(file_name, false, true, expected_identifiers, expected_metadata, expected_field_values);
  }

  SECTION("update_xml is true => fields are updated in-place in the XML, and walker.getFieldValues() is empty") {
    const std::map<std::string, std::string> expected_field_values{};

    extractMappingsTestHelper(file_name, true, true, expected_identifiers, expected_metadata, expected_field_values);
  }
}

TEST_CASE("MetadataWalker extracts mappings correctly when the Sid is unknown and resolve=false", "[for_each]") {
  const std::string file_name = "resources/unknownsid.xml";

  const std::map<std::string, std::string> expected_identifiers{{"S-1-8-6-5-3-0-9", "S-1-8-6-5-3-0-9"}};

  using org::apache::nifi::minifi::wel::METADATA;
  const std::map<METADATA, std::string> expected_metadata{
      {METADATA::SOURCE, "Microsoft-Windows-Security-Auditing"},
      {METADATA::TIME_CREATED, "event_timestamp"},
      {METADATA::EVENTID, "4672"},
      {METADATA::EVENT_RECORDID, "2575952"}};

  const std::map<std::string, std::string> expected_field_values{};

  SECTION("update_xml is false") {
    extractMappingsTestHelper(file_name, false, false, expected_identifiers, expected_metadata, expected_field_values);
  }

  SECTION("update_xml is true") {
    extractMappingsTestHelper(file_name, true, false, expected_identifiers, expected_metadata, expected_field_values);
  }
}

TEST_CASE("MetadataWalker extracts mappings correctly when the Sid is unknown and resolve=true", "[for_each]") {
  const std::string file_name = "resources/unknownsid.xml";

  const std::map<std::string, std::string> expected_identifiers{{"S-1-8-6-5-3-0-9", "S-1-8-6-5-3-0-9"}};

  using org::apache::nifi::minifi::wel::METADATA;
  const std::map<METADATA, std::string> expected_metadata{
      {METADATA::LOG_NAME, "MetadataWalkerTests"},
      {METADATA::SOURCE, "Microsoft-Windows-Security-Auditing"},
      {METADATA::TIME_CREATED, "event_timestamp"},
      {METADATA::EVENTID, "4672"},
      {METADATA::OPCODE, "event_data_for_flag_4"},
      {METADATA::EVENT_RECORDID, "2575952"},
      {METADATA::EVENT_TYPE, "178"},
      {METADATA::TASK_CATEGORY, "event_data_for_flag_3"},
      {METADATA::LEVEL, "event_data_for_flag_2"},
      {METADATA::KEYWORDS, "event_data_for_flag_5"}};

  SECTION("update_xml is false => fields are collected into walker.getFieldValues()") {
    const std::map<std::string, std::string> expected_field_values{
        {"Channel", "event_data_for_flag_6"},
        {"Keywords", "event_data_for_flag_5"},
        {"Level", "event_data_for_flag_2"},
        {"Opcode", "event_data_for_flag_4"},
        {"Task", "event_data_for_flag_3"}};

    extractMappingsTestHelper(file_name, false, true, expected_identifiers, expected_metadata, expected_field_values);
  }

  SECTION("update_xml is true => fields are updated in-place in the XML, and walker.getFieldValues() is empty") {
    const std::map<std::string, std::string> expected_field_values{};

    extractMappingsTestHelper(file_name, true, true, expected_identifiers, expected_metadata, expected_field_values);
  }
}
