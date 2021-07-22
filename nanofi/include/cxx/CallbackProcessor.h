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
#ifndef __CALLBACK_PROCESSOR_H__
#define __CALLBACK_PROCESSOR_H__

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
#include "io/BaseStream.h"
#include "FlowFileRecord.h"
#include "core/Processor.h"
#include "core/ProcessSession.h"
#include "core/Core.h"
#include "core/Resource.h"
#include "core/logging/LoggerConfiguration.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace processors {

// CallbackProcessor Class
class CallbackProcessor : public core::Processor {
 public:
  static core::Relationship Success;
  static core::Relationship Failure;
  // Constructor
  /*!
   * Create a new processor
   */
  CallbackProcessor(const std::string& name, const utils::Identifier& uuid = {})
      : Processor(std::move(name), uuid) {
  }
  // Destructor
  ~CallbackProcessor() override = default;
  // Processor Name
  static constexpr char const* ProcessorName = "CallbackProcessor";

 public:
  void setCallback(void *obj, std::function<void(core::ProcessSession*, core::ProcessContext *context)> ontrigger_callback,
                   std::function<void(core::ProcessContext *context)> onschedule_callback = {}) {
    objref_ = obj;
    ontrigger_callback_ = std::move(ontrigger_callback);
    onschedule_callback_ = std::move(onschedule_callback);
  }

  void onSchedule(core::ProcessContext *context, core::ProcessSessionFactory *sessionFactory) override;
  // OnTrigger method, implemented by MiNiFi CallbackProcessor
  void onTrigger(core::ProcessContext *context, core::ProcessSession *session) override;  // override;
  // Initialize, over write by NiFi CallbackProcessor
  void initialize() override;  // override;

  bool supportsDynamicProperties() override /*override*/ {
    return true;
  }

 protected:
  void *objref_{ nullptr };
  std::function<void(core::ProcessSession*, core::ProcessContext *context)> ontrigger_callback_;
  std::function<void(core::ProcessContext *context)> onschedule_callback_;
 private:
  // Logger
  std::shared_ptr<logging::Logger> logger_{ logging::LoggerFactory<CallbackProcessor>::getLogger() };
};

} /* namespace processors */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

#endif
