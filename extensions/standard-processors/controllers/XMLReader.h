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
#pragma once

#include "controllers/RecordSetReader.h"
#include "core/PropertyDefinitionBuilder.h"
#include "minifi-cpp/core/logging/Logger.h"
#include "core/logging/LoggerFactory.h"
#include "pugixml.hpp"

namespace org::apache::nifi::minifi::standard {

class XMLReader final : public core::RecordSetReaderImpl {
 public:
  using RecordSetReaderImpl::RecordSetReaderImpl;

  XMLReader(XMLReader&&) = delete;
  XMLReader(const XMLReader&) = delete;
  XMLReader& operator=(XMLReader&&) = delete;
  XMLReader& operator=(const XMLReader&) = delete;

  ~XMLReader() override = default;

  EXTENSIONAPI static constexpr const char* Description = "Reads XML content and creates Record objects. Records are expected in the second level of XML data, embedded in an enclosing root tag. "
      "Types for records are inferred automatically based on the content of the XML tags. For timestamps, the format is expected to be ISO 8601 compliant.";

  EXTENSIONAPI static constexpr auto FieldNameForContent = core::PropertyDefinitionBuilder<>::createProperty("Field Name for Content")
      .withDescription("If tags with content (e. g. <field>content</field>) are defined as nested records in the schema, the name of the tag will be used as name for the record and the value of "
        "this property will be used as name for the field. If the tag contains subnodes besides the content (e.g. <field>content<subfield>subcontent</subfield></field>), "
        "or a node attribute is present, we need to define a name for the text content, so that it can be distinguished from the subnodes. If this property is not set, the default "
        "name 'value' will be used for the text content of the tag in this case.")
      .build();
  EXTENSIONAPI static constexpr auto ParseXMLAttributes = core::PropertyDefinitionBuilder<>::createProperty("Parse XML Attributes")
      .withDescription("When this property is 'true' then XML attributes are parsed and added to the record as new fields, otherwise XML attributes and their values are ignored.")
      .isRequired(true)
      .withValidator(core::StandardPropertyValidators::BOOLEAN_VALIDATOR)
      .withDefaultValue("false")
      .build();
  EXTENSIONAPI static constexpr auto AttributePrefix = core::PropertyDefinitionBuilder<>::createProperty("Attribute Prefix")
      .withDescription("If this property is set, the name of attributes will be prepended with a prefix when they are added to a record.")
      .build();
  EXTENSIONAPI static constexpr auto ExpectRecordsAsArray = core::PropertyDefinitionBuilder<>::createProperty("Expect Records as Array")
      .withDescription("This property defines whether the reader expects a FlowFile to consist of a single Record or a series of Records with a \"wrapper element\". Because XML does not provide "
          "for a way to read a series of XML documents from a stream directly, it is common to combine many XML documents by concatenating them and then wrapping the entire XML blob "
          "with a \"wrapper element\". This property dictates whether the reader expects a FlowFile to consist of a single Record or a series of Records with a \"wrapper element\" "
          "that will be ignored.")
      .isRequired(true)
      .withValidator(core::StandardPropertyValidators::BOOLEAN_VALIDATOR)
      .withDefaultValue("false")
      .build();

  EXTENSIONAPI static constexpr auto Properties = std::array<core::PropertyReference, 4>{FieldNameForContent, ParseXMLAttributes, AttributePrefix, ExpectRecordsAsArray};

  EXTENSIONAPI static constexpr bool SupportsDynamicProperties = false;
  EXTENSIONAPI static constexpr auto ImplementsApis = std::array{ RecordSetReader::ProvidesApi };

  nonstd::expected<core::RecordSet, std::error_code> read(io::InputStream& input_stream) override;

  void initialize() override {
    setSupportedProperties(Properties);
  }
  void onEnable() override;

 private:
  void writeRecordField(core::RecordObject& record_object, const std::string& name, const std::string& value, bool write_pcdata_node = false) const;
  void parseNodeElement(core::RecordObject& record_object, const pugi::xml_node& node) const;
  void parseXmlNode(core::RecordObject& record_object, const pugi::xml_node& node) const;
  void addRecordFromXmlNode(const pugi::xml_node& node, core::RecordSet& record_set) const;
  bool parseRecordsFromXml(core::RecordSet& record_set, const std::string& xml_content) const;

  std::string field_name_for_content_;
  bool parse_xml_attributes_ = false;
  std::string attribute_prefix_;
  bool expect_records_as_array_ = false;
};

}  // namespace org::apache::nifi::minifi::standard
