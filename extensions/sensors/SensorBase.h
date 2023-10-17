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

#include <memory>
#include <string>
#include <utility>

#include "core/Processor.h"
#include "core/ProcessSession.h"
#include "io/StreamPipe.h"
#include "RTIMULib.h"
#include "RTMath.h"

namespace org::apache::nifi::minifi::processors {

class SensorBase : public core::Processor {
 public:
  explicit SensorBase(std::string name, const utils::Identifier& uuid = {})
    : Processor(std::move(name), uuid) {
  }
  ~SensorBase() override;

  void onTrigger(core::ProcessContext& context, core::ProcessSession& session) override;
  void initialize() override;
  void onSchedule(core::ProcessContext& context, core::ProcessSessionFactory& session_factory) override;

 protected:
  std::optional<RTIMUSettings> settings_;
  std::unique_ptr<RTIMU> imu_;
  std::shared_ptr<core::logging::Logger> logger_ = core::logging::LoggerFactory<SensorBase>::getLogger();
};

}  // namespace org::apache::nifi::minifi::processors
