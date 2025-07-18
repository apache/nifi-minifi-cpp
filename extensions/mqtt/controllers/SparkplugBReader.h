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
#include "core/logging/LoggerFactory.h"

namespace org::apache::nifi::minifi::controllers {

class SparkplugBReader final : public core::RecordSetReaderImpl {
 public:
  explicit SparkplugBReader(const std::string_view name, const utils::Identifier& uuid = {}) : RecordSetReaderImpl(name, uuid) {}

  SparkplugBReader(SparkplugBReader&&) = delete;
  SparkplugBReader(const SparkplugBReader&) = delete;
  SparkplugBReader& operator=(SparkplugBReader&&) = delete;
  SparkplugBReader& operator=(const SparkplugBReader&) = delete;

  ~SparkplugBReader() override = default;

  EXTENSIONAPI static constexpr const char* Description = "Reads Sparkplug B messages and turns them into individual Record objects. "
      "The reader expects a single Sparkplug B payload in a read operation, which is a protobuf-encoded binary message. This reader is typically used with MQTT processors like ConsumeMQTT.";

  EXTENSIONAPI static constexpr auto Properties = std::array<core::PropertyReference, 0>{};

  EXTENSIONAPI static constexpr bool SupportsDynamicProperties = false;
  ADD_COMMON_VIRTUAL_FUNCTIONS_FOR_CONTROLLER_SERVICES

  nonstd::expected<core::RecordSet, std::error_code> read(io::InputStream& input_stream) override;

  void initialize() override {
    setSupportedProperties(Properties);
  }
  void onEnable() override {}
  void yield() override {}
  bool isRunning() const override { return getState() == core::controller::ControllerServiceState::ENABLED; }
  bool isWorkAvailable() override { return false; }

 private:
  std::shared_ptr<core::logging::Logger> logger_ = core::logging::LoggerFactory<SparkplugBReader>::getLogger();
};

}  // namespace org::apache::nifi::minifi::controllers
