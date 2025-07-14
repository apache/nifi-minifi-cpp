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

#include "minifi-cpp/core/Record.h"
#include "controllers/RecordSetWriter.h"
#include "core/logging/LoggerFactory.h"

namespace org::apache::nifi::minifi::controllers {

class SparkplugBWriter final : public core::RecordSetWriterImpl {
 public:
  explicit SparkplugBWriter(const std::string_view name, const utils::Identifier& uuid = {}) : core::RecordSetWriterImpl(name, uuid) {}

  SparkplugBWriter(SparkplugBWriter&&) = delete;
  SparkplugBWriter(const SparkplugBWriter&) = delete;
  SparkplugBWriter& operator=(SparkplugBWriter&&) = delete;
  SparkplugBWriter& operator=(const SparkplugBWriter&) = delete;

  ~SparkplugBWriter() override = default;

  EXTENSIONAPI static constexpr const char* Description = "Serializes recordset to Sparkplug B messages and writes them into a FlowFile. "
      "This writer is typically used with MQTT processors like PublishMQTT.";

  EXTENSIONAPI static constexpr auto Properties = std::array<core::PropertyReference, 0>{};

  EXTENSIONAPI static constexpr bool SupportsDynamicProperties = false;
  ADD_COMMON_VIRTUAL_FUNCTIONS_FOR_CONTROLLER_SERVICES

  void write(const core::RecordSet& record_set, const std::shared_ptr<core::FlowFile>& flow_file, core::ProcessSession& session) override;

  void initialize() override {
    setSupportedProperties(Properties);
  }
  void onEnable() override {}
  void yield() override {}
  bool isRunning() const override { return getState() == core::controller::ControllerServiceState::ENABLED; }
  bool isWorkAvailable() override { return false; }

 private:
  std::shared_ptr<core::logging::Logger> logger_ = core::logging::LoggerFactory<SparkplugBWriter>::getLogger();
};

}  // namespace org::apache::nifi::minifi::controllers
