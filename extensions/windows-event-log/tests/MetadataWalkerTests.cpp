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

#include <fstream>
#include <map>
#include <memory>
#include <utility>
#include <string>
#include <set>

#include "TestBase.h"
#include "core/Core.h"
#include "wel/MetadataWalker.h"
#include "wel/XMLString.h"
#include "pugixml.hpp"

using MetadataWalker = org::apache::nifi::minifi::wel::MetadataWalker;
using XmlString = org::apache::nifi::minifi::wel::XmlString;

namespace {

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
  std::ifstream file(file_name);
  return std::string{std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}

}  // namespace

TEST_CASE("MetadataWalker updates the Sid in the XML if both update_xml and resolve are true", "[updateXmlMetadata]") {
  std::string xml = readFile("resources/nobodysid.xml");

  SECTION("No resolution") {
    REQUIRE(MetadataWalker::updateXmlMetadata(xml, 0x00, 0x00, false, true) == formatXml(xml));
  }

  SECTION("Resolve nobody") {
    std::string nobody = readFile("resources/withsids.xml");
    REQUIRE(MetadataWalker::updateXmlMetadata(xml, 0x00, 0x00, true, true, ".*Sid") == formatXml(nobody));
  }
}

TEST_CASE("MetadataWalker works even when there is no Data block", "[updateXmlMetadata]") {
  std::string xml = readFile("resources/nodata.xml");

  REQUIRE(MetadataWalker::updateXmlMetadata(xml, 0x00, 0x00, false, true) == formatXml(xml));
}

TEST_CASE("MetadataWalker throws if the input XML is invalid", "[updateXmlMetadata]") {
  std::string xml = readFile("resources/invalidxml.xml");

  REQUIRE_THROWS(MetadataWalker::updateXmlMetadata(xml, 0x00, 0x00, false, true) == formatXml(xml));
}

TEST_CASE("MetadataWalker will leave a Sid unchanged if it doesn't correspond to a user", "[updateXmlMetadata]") {
  std::string xml = readFile("resources/unknownsid.xml");

  REQUIRE(MetadataWalker::updateXmlMetadata(xml, 0x00, 0x00, false, true) == formatXml(xml));
  REQUIRE(MetadataWalker::updateXmlMetadata(xml, 0x00, 0x00, true, true) == formatXml(xml));
  REQUIRE(MetadataWalker::updateXmlMetadata(xml, 0x00, 0x00, true, true, ".*Sid") == formatXml(xml));
}

TEST_CASE("MetadataWalker can replace multiple Sids", "[updateXmlMetadata]") {
  std::string xml = readFile("resources/multiplesids.xml");

  std::string programmaticallyResolved;

  pugi::xml_document doc;
  xml = MetadataWalker::updateXmlMetadata(xml, 0x00, 0x00, false, true);
  pugi::xml_parse_result result = doc.load_string(xml.c_str());

  for (const auto &node : doc.child("Event").child("EventData").children()) {
    auto name = node.attribute("Name").as_string();
    if (utils::StringUtils::equalsIgnoreCase("GroupMembership", name)) {
      programmaticallyResolved = node.text().get();
      break;
    }
  }

  std::string expected = "Nobody Everyone Null Authority";

  // we are only testing mulitiple sid resolutions, not the resolution of other items.
  REQUIRE(expected == programmaticallyResolved);
}
