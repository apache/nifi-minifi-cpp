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
#include <utility>

#include "utils/ByteArrayCallback.h"
#include "FlowFileRecord.h"
#include "core/Processor.h"
#include "core/ProcessSession.h"
#include "core/Core.h"
#include "core/Property.h"
#include "utils/Id.h"
#include "RTIMULib.h"
#include "RTMath.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace processors {

// SensorBase Class
class SensorBase : public core::Processor {
 public:
  // Constructor
  /*!
   * Create a new processor
   */
  explicit SensorBase(const std::string& name, const utils::Identifier& uuid = {})
      : Processor(name, uuid),
        imu(nullptr),
        logger_(logging::LoggerFactory<SensorBase>::getLogger()) {
  }
  // Destructor
  ~SensorBase() override;
  // Processor Name
  static core::Relationship Success;
  // Supported Properties

  void onTrigger(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSession> &session) override;
  void initialize() override;
  void onSchedule(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSessionFactory> &sessionFactory) override;

  class WriteCallback : public OutputStreamCallback {
   public:
    explicit WriteCallback(std::string data)
        : data_{std::move(data)} {
    }
    std::string data_;
    int64_t process(const std::shared_ptr<io::BaseStream>& stream) override {
      if (data_.empty()) return 0;
      const auto write_ret = stream->write(reinterpret_cast<const uint8_t*>(data_.data()), data_.size());
      return io::isError(write_ret) ? -1 : gsl::narrow<int64_t>(write_ret);
    }
  };

 protected:
  RTIMUSettings settings;
  std::unique_ptr<RTIMU> imu;
  std::shared_ptr<logging::Logger> logger_;
};

} /* namespace processors */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
