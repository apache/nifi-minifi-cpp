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

#include <string>
#include <memory>

#include "MQTTControllerService.h"
#include "core/ProcessSession.h"
#include "core/Core.h"
#include "core/Property.h"
#include "core/logging/LoggerConfiguration.h"
#include "ConvertBase.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace processors {

/**
 * Purpose: Converts update messages into the appropriate Restful call
 *
 * Justification: The other protocol classes are responsible for standard messaging, which carries most
 * heartbeat related activity; however, updates generally connect to a different source. This processor will
 * retrieve the updates and respond via MQTT.
 */
class ConvertUpdate : public ConvertBase {
 public:
  // Constructor
  /*!
   * Create a new processor
   */
  explicit ConvertUpdate(const std::string& name, const utils::Identifier& uuid = {})
    : ConvertBase(name, uuid), logger_(logging::LoggerFactory<ConvertUpdate>::getLogger()) {
  }
  // Destructor
  virtual ~ConvertUpdate() = default;

  static core::Property SSLContext;
  // Processor Name
  static constexpr char const* ProcessorName = "ConvertUpdate";

 public:
  /**
     * Initialization of the processor
     */
    void initialize() override;
  /**
   * Function that's executed when the processor is triggered.
   * @param context process context.
   * @param sessionFactory process session factory that is used when creating
   * ProcessSession objects.
   */

  void onTrigger(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSession> &session) override;

 protected:
  std::shared_ptr<minifi::controllers::SSLContextService> ssl_context_service_;

 private:
  std::shared_ptr<logging::Logger> logger_;
};

} /* namespace processors */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
