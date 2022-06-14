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
#pragma once

#include <string>
#include <memory>

#include "MQTTControllerService.h"
#include "core/ProcessSession.h"
#include "core/Core.h"
#include "core/Property.h"
#include "core/logging/LoggerConfiguration.h"
#include "ConvertBase.h"
#include "utils/ArrayUtils.h"

namespace org::apache::nifi::minifi::processors {

/**
 * Purpose: Converts update messages into the appropriate Restful call
 *
 * Justification: The other protocol classes are responsible for standard messaging, which carries most
 * heartbeat related activity; however, updates generally connect to a different source. This processor will
 * retrieve the updates and respond via MQTT.
 */
class ConvertUpdate : public ConvertBase {
 public:
  explicit ConvertUpdate(const std::string& name, const utils::Identifier& uuid = {})
    : ConvertBase(name, uuid) {
  }
  ~ConvertUpdate() override = default;

  EXTENSIONAPI static core::Property SSLContext;
  static auto properties() { return utils::array_cat(ConvertBase::properties(), std::array{SSLContext}); }

  EXTENSIONAPI static constexpr bool SupportsDynamicProperties = false;
  EXTENSIONAPI static constexpr bool SupportsDynamicRelationships = false;
  EXTENSIONAPI static constexpr core::annotation::Input InputRequirement = core::annotation::Input::INPUT_ALLOWED;
  EXTENSIONAPI static constexpr bool IsSingleThreaded = false;

  ADD_COMMON_VIRTUAL_FUNCTIONS_FOR_PROCESSORS

  void initialize() override;
  void onTrigger(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSession> &session) override;

 protected:
  std::shared_ptr<minifi::controllers::SSLContextService> ssl_context_service_;

 private:
  std::shared_ptr<core::logging::Logger> logger_ = core::logging::LoggerFactory<ConvertUpdate>::getLogger();
};

}  // namespace org::apache::nifi::minifi::processors
