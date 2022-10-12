/**
 * @file GetGPS.h
 * GetGPS class declaration
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
#pragma once

#include <string>
#include <memory>
#include <utility>

#include "../FlowFileRecord.h"
#include "../core/Processor.h"

namespace org::apache::nifi::minifi::processors {

class GetGPS : public core::Processor {
 public:
  explicit GetGPS(std::string name, const utils::Identifier& uuid = {})
      : core::Processor(std::move(name), uuid) {
    gpsdHost_ = "localhost";
    gpsdPort_ = "2947";
    gpsdWaitTime_ = 50000000;
  }
  ~GetGPS() override = default;

  EXTENSIONAPI static constexpr const char* Description = "Obtains GPS coordinates from the GPSDHost and port.";

  EXTENSIONAPI static const core::Property GPSDHost;
  EXTENSIONAPI static const core::Property GPSDPort;
  EXTENSIONAPI static const core::Property GPSDWaitTime;
  static auto properties() {
    return std::array{
      GPSDHost,
      GPSDPort,
      GPSDWaitTime
    };
  }

  EXTENSIONAPI static const core::Relationship Success;
  static auto relationships() { return std::array{Success}; }

  EXTENSIONAPI static constexpr bool SupportsDynamicProperties = false;
  EXTENSIONAPI static constexpr bool SupportsDynamicRelationships = false;
  EXTENSIONAPI static constexpr core::annotation::Input InputRequirement = core::annotation::Input::INPUT_ALLOWED;
  EXTENSIONAPI static constexpr bool IsSingleThreaded = false;

  ADD_COMMON_VIRTUAL_FUNCTIONS_FOR_PROCESSORS

  void onSchedule(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSessionFactory> &sessionFactory) override;
  void onTrigger(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSession> &session) override;
  void initialize() override;

 private:
  std::string gpsdHost_;
  std::string gpsdPort_;
  int64_t gpsdWaitTime_;
  std::shared_ptr<core::logging::Logger> logger_ = core::logging::LoggerFactory<GetGPS>::getLogger();
};

}  // namespace org::apache::nifi::minifi::processors
