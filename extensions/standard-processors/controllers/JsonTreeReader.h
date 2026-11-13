// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#pragma once

#include "controllers/RecordSetReader.h"

namespace org::apache::nifi::minifi::standard {

class JsonTreeReader final : public core::RecordSetReaderImpl {
 public:
  using RecordSetReaderImpl::RecordSetReaderImpl;

  JsonTreeReader(JsonTreeReader&&) = delete;
  JsonTreeReader(const JsonTreeReader&) = delete;
  JsonTreeReader& operator=(JsonTreeReader&&) = delete;
  JsonTreeReader& operator=(const JsonTreeReader&) = delete;

  ~JsonTreeReader() override = default;

  EXTENSIONAPI static constexpr const char* Description = "Parses JSON into individual Record objects. "
    "While the reader expects each record to be well-formed JSON, the content of a FlowFile may consist of many records, "
    "each as a well-formed JSON array or JSON object with optional whitespace between them, such as the common 'JSON-per-line' format. "
    "If an array is encountered, each element in that array will be treated as a separate record. "
    "If the schema that is configured contains a field that is not present in the JSON, a null value will be used. "
    "If the JSON contains a field that is not present in the schema, that field will be skipped.";

  EXTENSIONAPI static constexpr auto Properties = std::array<core::PropertyReference, 0>{};

  EXTENSIONAPI static constexpr bool SupportsDynamicProperties = false;
  EXTENSIONAPI static constexpr auto ImplementsApis = std::array{ RecordSetReader::ProvidesApi };
  ADD_COMMON_VIRTUAL_FUNCTIONS_FOR_CONTROLLER_SERVICES

  std::expected<core::RecordSet, std::error_code> read(io::InputStream& input_stream) override;

  void initialize() override {
    setSupportedProperties(Properties);
  }
  void onEnable() override {}
};

}  // namespace org::apache::nifi::minifi::standard
