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
  CallbackProcessor(std::string name, utils::Identifier uuid = utils::Identifier())
      : Processor(name, uuid),
        callback_(nullptr),
        objref_(nullptr),
        logger_(logging::LoggerFactory<CallbackProcessor>::getLogger()) {
  }
  // Destructor
  virtual ~CallbackProcessor() {

  }
  // Processor Name
  static constexpr char const* ProcessorName = "CallbackProcessor";

 public:

  void setCallback(void *obj,std::function<void(core::ProcessSession*, core::ProcessContext *context)> ontrigger_callback) {
    objref_ = obj;
    callback_ = ontrigger_callback;
  }

  // OnTrigger method, implemented by NiFi CallbackProcessor
  virtual void onTrigger(core::ProcessContext *context, core::ProcessSession *session);  // override;
  // Initialize, over write by NiFi CallbackProcessor
  virtual void initialize();  // override;

  virtual bool supportsDynamicProperties() /*override*/ {
    return true;
  }

 protected:
  void *objref_;
  std::function<void(core::ProcessSession*, core::ProcessContext *context)> callback_;
 private:
  // Logger
  std::shared_ptr<logging::Logger> logger_;

};

REGISTER_RESOURCE(CallbackProcessor, "");

} /* namespace processors */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

#endif
