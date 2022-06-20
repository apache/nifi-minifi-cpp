/**
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
#include <string>
#include "core/ProcessContext.h"
#include "ConvertBase.h"
#include "c2/PayloadSerializer.h"

namespace org::apache::nifi::minifi::processors {

const core::Relationship ConvertBase::Success("success", "All files are routed to success");

void ConvertBase::initialize() {
  setSupportedProperties(properties());
  setSupportedRelationships(relationships());
}

void ConvertBase::onSchedule(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSessionFactory>& /*sessionFactory*/) {
  std::string controller_service_name;
  if (context->getProperty(MQTTControllerService.getName(), controller_service_name) && !controller_service_name.empty()) {
    auto service = context->getControllerService(controller_service_name);
    mqtt_service_ = std::static_pointer_cast<controllers::MQTTControllerService>(service);
  }
  context->getProperty(ListeningTopic.getName(), listening_topic);
  if (!listening_topic.empty()) {
    mqtt_service_->subscribeToTopic(listening_topic);
  }
}

}  // namespace org::apache::nifi::minifi::processors
