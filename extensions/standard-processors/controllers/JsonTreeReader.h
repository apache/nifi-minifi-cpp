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

  nonstd::expected<core::RecordSet, std::error_code> read(io::InputStream& input_stream) override;

  void initialize() override {
    setSupportedProperties(Properties);
  }
  void onEnable() override {}
};

}  // namespace org::apache::nifi::minifi::standard
