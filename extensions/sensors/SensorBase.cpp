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
#include "SensorBase.h"
#include "io/DataStream.h"
#include "io/StreamFactory.h"
#include "ResourceClaim.h"
#include "utils/StringUtils.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace processors {

core::Relationship SensorBase::Success("success", "All files are routed to success");

void SensorBase::initialize() {
}

void SensorBase::onSchedule(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSessionFactory> &sessionFactory) {
  imu = std::unique_ptr<RTIMU>(RTIMU::createIMU(&settings));
  if (imu) {
    imu->IMUInit();
    imu->setGyroEnable(true);
    imu->setAccelEnable(true);
  } else {
    throw std::runtime_error("RTIMU could not be initialized");
  }

}

SensorBase::~SensorBase() {
}

void SensorBase::onTrigger(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSession> &session) {
}

} /* namespace processors */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
