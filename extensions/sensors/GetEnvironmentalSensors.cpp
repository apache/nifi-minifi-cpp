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
#include <regex.h>
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
#include "GetEnvironmentalSensors.h"
#include "io/BufferStream.h"
#include "io/StreamFactory.h"
#include "ResourceClaim.h"
#include "utils/StringUtils.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace processors {

std::shared_ptr<utils::IdGenerator> GetEnvironmentalSensors::id_generator_ = utils::IdGenerator::getIdGenerator();

const char *GetEnvironmentalSensors::ProcessorName = "GetEnvironmentalSensors";

core::Relationship GetEnvironmentalSensors::Success("success", "All files are routed to success");

void GetEnvironmentalSensors::initialize() {
  logger_->log_trace("Initializing EnvironmentalSensors");
  // Set the supported properties
  std::set<core::Property> properties;

  setSupportedProperties(properties);
  // Set the supported relationships
  std::set<core::Relationship> relationships;
  relationships.insert(Success);
  setSupportedRelationships(relationships);
}

void GetEnvironmentalSensors::onSchedule(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSessionFactory> &sessionFactory) {
  SensorBase::onSchedule(context, sessionFactory);

  humidity_sensor_ = RTHumidity::createHumidity(&settings);
  if (humidity_sensor_) {
    humidity_sensor_->humidityInit();
  } else {
    throw std::runtime_error("RTHumidity could not be initialized");
  }

  pressure_sensor_ = RTPressure::createPressure(&settings);
  if (pressure_sensor_) {
    pressure_sensor_->pressureInit();
  } else {
    throw std::runtime_error("RTPressure could not be initialized");
  }
}

void GetEnvironmentalSensors::notifyStop() {
  delete humidity_sensor_;
  delete pressure_sensor_;
}

GetEnvironmentalSensors::~GetEnvironmentalSensors() = default;

void GetEnvironmentalSensors::onTrigger(const std::shared_ptr<core::ProcessContext>& /*context*/, const std::shared_ptr<core::ProcessSession>& session) {
  auto flow_file_ = session->create();

  if (imu->IMURead()) {
    RTIMU_DATA imuData = imu->getIMUData();
    auto vector = imuData.accel;
    std::string degrees = RTMath::displayDegrees("acceleration", vector);
    flow_file_->addAttribute("ACCELERATION", degrees);
  } else {
    logger_->log_trace("IMURead returned false. Could not gather acceleration");
  }

  RTIMU_DATA data;

  bool have_sensor = false;

  if (humidity_sensor_->humidityRead(data)) {
    if (data.humidityValid) {
      have_sensor = true;
      std::stringstream ss;
      ss << std::fixed << std::setprecision(2) << data.humidity;
      flow_file_->addAttribute("HUMIDITY", ss.str());
    }
  } else {
    logger_->log_trace("Could not read humidity sensors");
  }

  if (pressure_sensor_->pressureRead(data)) {
    if (data.pressureValid) {
      have_sensor = true;
      {
        std::stringstream ss;
        ss << std::fixed << std::setprecision(2) << data.pressure;
        flow_file_->addAttribute("PRESSURE", ss.str());
      }

      if (data.temperatureValid) {
        std::stringstream ss;
        ss << std::fixed << std::setprecision(2) << data.temperature;
        flow_file_->addAttribute("TEMPERATURE", ss.str());
      }
    }
  } else {
    logger_->log_trace("Could not read pressure sensors");
  }

  if (have_sensor) {
    WriteCallback callback("GetEnvironmentalSensors");
    session->write(flow_file_, &callback);
    session->transfer(flow_file_, Success);
  }
}

REGISTER_RESOURCE(GetEnvironmentalSensors, "Provides sensor information from known sensors to include humidity and pressure data");

} /* namespace processors */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
