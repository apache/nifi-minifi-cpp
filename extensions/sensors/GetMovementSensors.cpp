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
#include <memory>
#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <iterator>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "utils/ByteArrayCallback.h"
#include "core/FlowFile.h"
#include "core/logging/Logger.h"
#include "core/ProcessContext.h"
#include "core/Relationship.h"
#include "core/Resource.h"
#include "GetMovementSensors.h"
#include "io/BufferStream.h"
#include "io/StreamFactory.h"
#include "ResourceClaim.h"
#include "utils/StringUtils.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace processors {

const char *GetMovementSensors::ProcessorName = "GetMovementSensors";

core::Relationship GetMovementSensors::Success("success", "All files are routed to success");

void GetMovementSensors::initialize() {
  logger_->log_trace("Initializing GetMovementSensors");
  // Set the supported properties
  std::set<core::Property> properties;

  setSupportedProperties(properties);
  // Set the supported relationships
  std::set<core::Relationship> relationships;
  relationships.insert(Success);
  setSupportedRelationships(relationships);
}

GetMovementSensors::~GetMovementSensors() = default;

void GetMovementSensors::onTrigger(const std::shared_ptr<core::ProcessContext>& /*context*/, const std::shared_ptr<core::ProcessSession>& session) {
  auto flow_file_ = session->create();

  if (imu->IMURead()) {
    RTIMU_DATA imuData = imu->getIMUData();

    if (imuData.accelValid) {
      auto vector = imuData.accel;
      std::string degrees = RTMath::displayDegrees("acceleration", vector);
      flow_file_->addAttribute("ACCELERATION", degrees);
    } else {
      logger_->log_trace("Could not read accelerometer");
    }
    if (imuData.gyroValid) {
      auto vector = imuData.gyro;
      std::string degrees = RTMath::displayDegrees("gyro", vector);
      flow_file_->addAttribute("GYRO", degrees);
    } else {
      logger_->log_trace("Could not read gyroscope");
    }

    WriteCallback callback("GetMovementSensors");
    session->write(flow_file_, &callback);
    session->transfer(flow_file_, Success);
  }
}

REGISTER_RESOURCE(GetMovementSensors, "Defines a processor that is able to retrieve sensor information from a class of known servo sensors");

} /* namespace processors */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
