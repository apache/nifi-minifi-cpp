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
#include <set>

#include "core/ProcessContext.h"
#include "core/Relationship.h"
#include "SensorBase.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace processors {

void SensorBase::initialize() {
}

void SensorBase::onSchedule(const std::shared_ptr<core::ProcessContext>& /*context*/, const std::shared_ptr<core::ProcessSessionFactory>& /*sessionFactory*/) {
  // Deferred instantiation of RTIMUSettings, because it can create a file "RTIMULib.ini" in the working directory.
  // SensorBase is instantiated when creating the manifest.
  settings_ = std::make_optional<RTIMUSettings>();

  imu_ = std::unique_ptr<RTIMU>(RTIMU::createIMU(&settings_.value()));
  if (imu_) {
    imu_->IMUInit();
    imu_->setGyroEnable(true);
    imu_->setAccelEnable(true);
  } else {
    throw std::runtime_error("RTIMU could not be initialized");
  }
}

SensorBase::~SensorBase() = default;

void SensorBase::onTrigger(const std::shared_ptr<core::ProcessContext>& /*context*/, const std::shared_ptr<core::ProcessSession>& /*session*/) {
}

} /* namespace processors */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
