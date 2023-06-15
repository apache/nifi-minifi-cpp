/**
 * @file CallbackProcessor.h
 * CallbackProcessor class declaration
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

#include <stdio.h>
#include <string>
#include <errno.h>
#include <chrono>
#include <thread>
#include <functional>
#include <iostream>
#include <utility>
#include <sys/types.h>
#include "core/cstructs.h"
#include "FlowFileRecord.h"
#include "core/Processor.h"
#include "core/ProcessSession.h"
#include "core/PropertyDefinition.h"
#include "core/RelationshipDefinition.h"
#include "core/Core.h"
#include "core/logging/LoggerConfiguration.h"

namespace org::apache::nifi::minifi::processors {

class CallbackProcessor : public core::Processor {
 public:
  static constexpr const char* Description = "";
  static constexpr auto Properties = std::array<core::PropertyReference, 0>{};
  static constexpr auto Success = core::RelationshipDefinition{"success", "All files are routed to success"};
  static constexpr auto Failure = core::RelationshipDefinition{"failure", "Failed files (based on callback logic) are transferred to failure"};
  static constexpr auto Relationships = std::array{Success, Failure};
  static constexpr bool SupportsDynamicProperties = true;
  static constexpr bool SupportsDynamicRelationships = false;
  static constexpr core::annotation::Input InputRequirement = core::annotation::Input::INPUT_ALLOWED;
  static constexpr bool IsSingleThreaded = false;

  ADD_COMMON_VIRTUAL_FUNCTIONS_FOR_PROCESSORS

  explicit CallbackProcessor(std::string name, const utils::Identifier& uuid = {})
      : Processor(std::move(name), uuid) {
  }
  ~CallbackProcessor() override = default;

  void setCallback(void *obj, std::function<void(core::ProcessSession*, core::ProcessContext *context)> ontrigger_callback,
                   std::function<void(core::ProcessContext *context)> onschedule_callback = {}) {
    objref_ = obj;
    ontrigger_callback_ = std::move(ontrigger_callback);
    onschedule_callback_ = std::move(onschedule_callback);
  }

  void onSchedule(core::ProcessContext *context, core::ProcessSessionFactory *sessionFactory) override;
  void onTrigger(core::ProcessContext *context, core::ProcessSession *session) override;
  void initialize() override;

 protected:
  void *objref_{ nullptr };
  std::function<void(core::ProcessSession*, core::ProcessContext *context)> ontrigger_callback_;
  std::function<void(core::ProcessContext *context)> onschedule_callback_;
 private:
  std::shared_ptr<core::logging::Logger> logger_{ core::logging::LoggerFactory<CallbackProcessor>::getLogger(uuid_) };
};

}  // namespace org::apache::nifi::minifi::processors
