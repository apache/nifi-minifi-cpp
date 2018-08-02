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
#ifndef EXTENSIONS_SENSORS_SENSORBASE_H_
#define EXTENSIONS_SENSORS_SENSORBASE_H_



#include <memory>
#include <regex>

#include "utils/ByteArrayCallback.h"
#include "FlowFileRecord.h"
#include "core/Processor.h"
#include "core/ProcessSession.h"
#include "core/Core.h"
#include "core/Property.h"
#include "core/Resource.h"
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
  SensorBase(std::string name, utils::Identifier uuid = utils::Identifier())
      : Processor(name, uuid),
        imu(nullptr),
        logger_(logging::LoggerFactory<SensorBase>::getLogger()) {
  }
  // Destructor
  virtual ~SensorBase();
  // Processor Name
  static core::Relationship Success;
  // Supported Properties

  virtual void onTrigger(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSession> &session) override;
  virtual void initialize() override;
  virtual void onSchedule(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSessionFactory> &sessionFactory) override;

  class WriteCallback : public OutputStreamCallback {
     public:
      WriteCallback(std::string data)
          : _data(const_cast<char*>(data.data())),
            _dataSize(data.size()) {
      }
      char *_data;
      uint64_t _dataSize;
      int64_t process(std::shared_ptr<io::BaseStream> stream) {
        int64_t ret = 0;
        if (_data && _dataSize > 0)
          ret = stream->write(reinterpret_cast<uint8_t*>(_data), _dataSize);
        return ret;
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
#endif /* EXTENSIONS_SENSORS_SENSORBASE_H_ */
