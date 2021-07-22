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
#include "FlowFileRecord.h"
#include "core/Processor.h"
#include "core/ProcessSession.h"
#include "core/Core.h"
#include "core/Property.h"
#include "core/logging/LoggerConfiguration.h"
#include "MQTTClient.h"
#include "c2/protocols/RESTProtocol.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace processors {

/**
 * Purpose: Provides base functionality for mqtt conversion classes.
 * At a minimum we need a controller service and listening topic.
 */
class ConvertBase : public core::Processor, public minifi::c2::RESTProtocol {
 public:
  // Constructor
  /*!
   * Create a new processor
   */
  explicit ConvertBase(const std::string& name, const utils::Identifier& uuid = {})
      : core::Processor(name, uuid) {
  }
  // Destructor
  virtual ~ConvertBase() = default;
  // Supported Properties
  static core::Property MQTTControllerService;
  static core::Property ListeningTopic;

  static core::Relationship Success;

 public:
  /**
   * Initialization of the processor
   */
  void initialize() override;
  /**
   * Function that's executed when the processor is scheduled.
   * @param context process context.
   * @param sessionFactory process session factory that is used when creating
   * ProcessSession objects.
   */
  void onSchedule(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSessionFactory> &sessionFactory) override;

 protected:
  /**
   * MQTT controller service.
   */
  std::shared_ptr<controllers::MQTTControllerService> mqtt_service_;

  std::string listening_topic;
};

} /* namespace processors */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
