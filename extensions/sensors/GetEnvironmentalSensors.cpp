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
#include <iterator>
#include <map>
#include <string>

#include "core/logging/Logger.h"
#include "core/ProcessContext.h"
#include "core/Resource.h"
#include "GetEnvironmentalSensors.h"
#include "io/BufferStream.h"
#include "io/StreamFactory.h"

namespace org::apache::nifi::minifi::processors {

std::shared_ptr<utils::IdGenerator> GetEnvironmentalSensors::id_generator_ = utils::IdGenerator::getIdGenerator();

void GetEnvironmentalSensors::initialize() {
  logger_->log_trace("Initializing EnvironmentalSensors");

  setSupportedProperties(Properties);
  setSupportedRelationships(Relationships);
}

void GetEnvironmentalSensors::onSchedule(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSessionFactory> &sessionFactory) {
  SensorBase::onSchedule(context, sessionFactory);

  humidity_sensor_ = RTHumidity::createHumidity(&settings_.value());
  if (humidity_sensor_) {
    humidity_sensor_->humidityInit();
  } else {
    throw std::runtime_error("RTHumidity could not be initialized");
  }

  pressure_sensor_ = RTPressure::createPressure(&settings_.value());
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

  if (imu_->IMURead()) {
    RTIMU_DATA imuData = imu_->getIMUData();
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
    session->writeBuffer(flow_file_, "GetEnvironmentalSensors");
    session->transfer(flow_file_, Success);
  }
}

REGISTER_RESOURCE(GetEnvironmentalSensors, Processor);

}  // namespace org::apache::nifi::minifi::processors
