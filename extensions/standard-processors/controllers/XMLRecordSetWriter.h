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

#include "controllers/RecordSetWriter.h"
#include "core/PropertyDefinitionBuilder.h"
#include "minifi-cpp/core/logging/Logger.h"
#include "core/logging/LoggerFactory.h"
#include "pugixml.hpp"

namespace org::apache::nifi::minifi::standard {
enum class WrapElementsOfArraysOptions {
  UsePropertyAsWrapper,
  UsePropertyForElements,
  NoWrapping
};
}  // namespace org::apache::nifi::minifi::standard

namespace magic_enum::customize {
using WrapElementsOfArraysOptions = org::apache::nifi::minifi::standard::WrapElementsOfArraysOptions;

template <>
constexpr customize_t enum_name<WrapElementsOfArraysOptions>(WrapElementsOfArraysOptions value) noexcept {
  switch (value) {
    case WrapElementsOfArraysOptions::UsePropertyAsWrapper:
      return "Use Property as Wrapper";
    case WrapElementsOfArraysOptions::UsePropertyForElements:
      return "Use Property for Elements";
    case WrapElementsOfArraysOptions::NoWrapping:
      return "No Wrapping";
  }
  return invalid_tag;
}
}  // namespace magic_enum::customize

namespace org::apache::nifi::minifi::standard {

class XMLRecordSetWriter final : public core::RecordSetWriterImpl {
 public:
  using RecordSetWriterImpl::RecordSetWriterImpl;

  XMLRecordSetWriter(XMLRecordSetWriter&&) = delete;
  XMLRecordSetWriter(const XMLRecordSetWriter&) = delete;
  XMLRecordSetWriter& operator=(XMLRecordSetWriter&&) = delete;
  XMLRecordSetWriter& operator=(const XMLRecordSetWriter&) = delete;

  ~XMLRecordSetWriter() override = default;

  EXTENSIONAPI static constexpr const char* Description = "Writes a RecordSet to XML. The records are wrapped by a root tag.";

  EXTENSIONAPI static constexpr auto ArrayTagName = core::PropertyDefinitionBuilder<>::createProperty("Array Tag Name")
      .withDescription("Name of the tag used by property \"Wrap Elements of Arrays\" to write arrays")
      .build();
  EXTENSIONAPI static constexpr auto WrapElementsOfArrays = core::PropertyDefinitionBuilder<3>::createProperty("Wrap Elements of Arrays")
      .withDescription("Specifies how the writer wraps elements of fields of type array. If 'Use Property as Wrapper' is set, the property \"Array Tag Name\" will be used as the tag name to wrap "
          "elements of an array. The field name of the array field will be used for the tag name of the elements. If 'Use Property for Elements' is set, the property \"Array Tag Name\" will be "
          "used for the tag name of the elements of an array. The field name of the array field will be used as the tag name to wrap elements. If 'No Wrapping' is set, the elements of an array "
          "will not be wrapped.")
      .withDefaultValue(magic_enum::enum_name(WrapElementsOfArraysOptions::NoWrapping))
      .withAllowedValues(magic_enum::enum_names<WrapElementsOfArraysOptions>())
      .isRequired(true)
      .build();
  EXTENSIONAPI static constexpr auto OmitXMLDeclaration = core::PropertyDefinitionBuilder<>::createProperty("Omit XML Declaration")
      .withDescription("Specifies whether or not to include XML declaration")
      .isRequired(true)
      .withValidator(core::StandardPropertyValidators::BOOLEAN_VALIDATOR)
      .withDefaultValue("false")
      .build();
  EXTENSIONAPI static constexpr auto PrettyPrintXML = core::PropertyDefinitionBuilder<>::createProperty("Pretty Print XML")
      .withDescription("Specifies whether or not the XML should be pretty printed")
      .isRequired(true)
      .withValidator(core::StandardPropertyValidators::BOOLEAN_VALIDATOR)
      .withDefaultValue("false")
      .build();
  EXTENSIONAPI static constexpr auto NameOfRecordTag = core::PropertyDefinitionBuilder<>::createProperty("Name of Record Tag")
      .withDescription("Specifies the name of the XML record tag wrapping the record fields.")
      .withValidator(core::StandardPropertyValidators::NON_BLANK_VALIDATOR)
      .isRequired(true)
      .build();
  EXTENSIONAPI static constexpr auto NameOfRootTag = core::PropertyDefinitionBuilder<>::createProperty("Name of Root Tag")
      .withDescription("Specifies the name of the XML root tag wrapping the record set.")
      .withValidator(core::StandardPropertyValidators::NON_BLANK_VALIDATOR)
      .isRequired(true)
      .build();

  EXTENSIONAPI static constexpr auto Properties = std::array<core::PropertyReference, 6>{
    ArrayTagName, WrapElementsOfArrays, OmitXMLDeclaration, PrettyPrintXML, NameOfRecordTag, NameOfRootTag
  };

  EXTENSIONAPI static constexpr bool SupportsDynamicProperties = false;
  EXTENSIONAPI static constexpr auto ImplementsApis = std::array{ RecordSetWriter::ProvidesApi };

  void write(const core::RecordSet& record_set, const std::shared_ptr<core::FlowFile>& flow_file, core::ProcessSession& session) override;

  void initialize() override {
    setSupportedProperties(Properties);
  }
  void onEnable() override;

 private:
  std::string formatXmlOutput(pugi::xml_document& xml_doc) const;
  std::string convertRecordSetToXml(const core::RecordSet& record_set) const;
  void convertRecordArrayField(const std::string& field_name, const core::RecordField& field, pugi::xml_node& parent_node) const;
  void convertRecordField(const std::string& field_name, const core::RecordField& field, pugi::xml_node& parent_node) const;

  WrapElementsOfArraysOptions wrap_elements_of_arrays_ = WrapElementsOfArraysOptions::NoWrapping;
  std::string array_tag_name_;
  bool omit_xml_declaration_ = false;
  bool pretty_print_xml_ = false;
  std::string name_of_record_tag_;
  std::string name_of_root_tag_;
};

}  // namespace org::apache::nifi::minifi::standard
