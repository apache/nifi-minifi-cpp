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
#pragma once

#include <memory>
#include <regex>
#include <string>

#include "utils/ByteArrayCallback.h"
#include "FlowFileRecord.h"
#include "core/Processor.h"
#include "core/ProcessSession.h"
#include "core/Core.h"
#include "core/Property.h"
#include "utils/Id.h"
#include "RTIMULib.h"
#include "SensorBase.h"
#include "RTMath.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace processors {

// EnvironmentalSensors Class
class GetEnvironmentalSensors : public SensorBase {
 public:
  // Constructor
  /*!
   * Create a new processor
   */
  explicit GetEnvironmentalSensors(const std::string& name, const utils::Identifier& uuid = {})
      : SensorBase(name, uuid),
        humidity_sensor_(nullptr),
        pressure_sensor_(nullptr),
        logger_(logging::LoggerFactory<GetEnvironmentalSensors>::getLogger()) {
  }
  // Destructor
  ~GetEnvironmentalSensors() override;
  // Processor Name
  static const char *ProcessorName;
  static core::Relationship Success;
  // Supported Properties

  void onTrigger(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSession> &session) override;
  void initialize() override;
  void onSchedule(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSessionFactory> &sessionFactory) override;

 protected:
  void notifyStop() override;

 private:
  RTHumidity *humidity_sensor_;
  RTPressure *pressure_sensor_;
  std::shared_ptr<logging::Logger> logger_;
  static std::shared_ptr<utils::IdGenerator> id_generator_;
};

} /* namespace processors */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
