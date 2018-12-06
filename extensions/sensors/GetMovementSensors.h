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
#ifndef EXTENSIONS_SENSORS_GETMOVEMENT_H_
#define EXTENSIONS_SENSORS_GETMOVEMENT_H_



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
#include "SensorBase.h"
#include "RTIMULib.h"
#include "RTMath.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace processors {

// GetMovementSensors Class
class GetMovementSensors : public SensorBase {
 public:

  // Constructor
  /*!
   * Create a new processor
   */
  GetMovementSensors(std::string name, utils::Identifier uuid = utils::Identifier())
      : SensorBase(name, uuid),
        logger_(logging::LoggerFactory<GetMovementSensors>::getLogger()) {
  }
  // Destructor
  virtual ~GetMovementSensors();
  // Processor Name
  static const char *ProcessorName;
  static core::Relationship Success;
  // Supported Properties

  virtual void onTrigger(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSession> &session) override;
  virtual void initialize() override;

 private:
  std::shared_ptr<logging::Logger> logger_;
};

REGISTER_RESOURCE(GetMovementSensors, "Defines a processor that is able to retrieve sensor information from a class of known servo sensors")

} /* namespace processors */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
#endif /* EXTENSIONS_SENSORS_GETMOVEMENT_H_ */
