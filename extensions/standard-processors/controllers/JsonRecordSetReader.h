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
#include "core/FlowFile.h"
#include "core/ProcessSession.h"

namespace org::apache::nifi::minifi::standard {

class JsonRecordSetReader final : public core::RecordSetReaderImpl {
 public:
  explicit JsonRecordSetReader(const std::string_view name, const utils::Identifier& uuid = {}) : RecordSetReaderImpl(name, uuid) {}

  JsonRecordSetReader(JsonRecordSetReader&&) = delete;
  JsonRecordSetReader(const JsonRecordSetReader&) = delete;
  JsonRecordSetReader& operator=(JsonRecordSetReader&&) = delete;
  JsonRecordSetReader& operator=(const JsonRecordSetReader&) = delete;

  ~JsonRecordSetReader() override = default;

  EXTENSIONAPI static constexpr const char* Description = "Parses JSON into individual Record objects. "
    "While the reader expects each record to be well-formed JSON, the content of a FlowFile may consist of many records, "
    "each as a well-formed JSON array or JSON object with optional whitespace between them, such as the common 'JSON-per-line' format. "
    "If an array is encountered, each element in that array will be treated as a separate record. "
    "If the schema that is configured contains a field that is not present in the JSON, a null value will be used. "
    "If the JSON contains a field that is not present in the schema, that field will be skipped.";

  EXTENSIONAPI static constexpr auto Properties = std::array<core::PropertyReference, 0>{};

  EXTENSIONAPI static constexpr bool SupportsDynamicProperties = false;
  ADD_COMMON_VIRTUAL_FUNCTIONS_FOR_CONTROLLER_SERVICES

  nonstd::expected<core::RecordSet, std::error_code> read(const std::shared_ptr<core::FlowFile>& flow_file, core::ProcessSession& session) override;

  void initialize() override {
    setSupportedProperties(Properties);
  }
  void onEnable() override {}
  void yield() override {}
  bool isRunning() const override { return getState() == core::controller::ControllerServiceState::ENABLED; }
  bool isWorkAvailable() override { return false; }
};

}  // namespace org::apache::nifi::minifi::standard
