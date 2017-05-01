/**
 * @file SchedulingAgent.h
 * SchedulingAgent class declaration
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
#ifndef __SCHEDULING_AGENT_H__
#define __SCHEDULING_AGENT_H__

#include <uuid/uuid.h>
#include <vector>
#include <map>
#include <mutex>
#include <atomic>
#include <algorithm>
#include <thread>
#include "utils/TimeUtil.h"
#include "core/Core.h"
#include "core/logging/Logger.h"
#include "properties/Configure.h"
#include "FlowFileRecord.h"
#include "core/logging/Logger.h"
#include "core/Processor.h"
#include "core/ProcessContext.h"
#include "provenance/ProvenanceRepository.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {

// SchedulingAgent Class
class SchedulingAgent {
 public:
  // Constructor
  /*!
   * Create a new processor
   */
  SchedulingAgent(std::shared_ptr<core::Repository> repo) {
    logger_ = logging::Logger::getLogger();
    running_ = false;
    repo_ = repo;
  }
  // Destructor
  virtual ~SchedulingAgent() {

  }
  // onTrigger, return whether the yield is need
  bool onTrigger(std::shared_ptr<core::Processor> processor,
                 core::ProcessContext *processContext,
                 core::ProcessSessionFactory *sessionFactory);
  // Whether agent has work to do
  bool hasWorkToDo(std::shared_ptr<core::Processor> processor);
  // Whether the outgoing need to be backpressure
  bool hasTooMuchOutGoing(std::shared_ptr<core::Processor> processor);
  // start
  void start() {
    running_ = true;
  }
  // stop
  void stop() {
    running_ = false;
  }

 public:
  // schedule, overwritten by different DrivenSchedulingAgent
  virtual void schedule(std::shared_ptr<core::Processor> processor) = 0;
  // unschedule, overwritten by different DrivenSchedulingAgent
  virtual void unschedule(std::shared_ptr<core::Processor> processor) = 0;

  SchedulingAgent(const SchedulingAgent &parent) = delete;
  SchedulingAgent &operator=(const SchedulingAgent &parent) = delete;
 protected:
  // Logger
  std::shared_ptr<logging::Logger> logger_;
  // Mutex for protection
  std::mutex mutex_;
  // Whether it is running
  std::atomic<bool> running_;
  // AdministrativeYieldDuration
  int64_t _administrativeYieldDuration;
  // BoredYieldDuration
  int64_t _boredYieldDuration;

  std::shared_ptr<core::Repository> repo_;

 private:
  // Prevent default copy constructor and assignment operation
  // Only support pass by reference or pointer

};

} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
#endif
