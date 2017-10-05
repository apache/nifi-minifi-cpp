/**
 * @file TimerDrivenSchedulingAgent.h
 * TimerDrivenSchedulingAgent class declaration
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
#ifndef __TIMER_DRIVEN_SCHEDULING_AGENT_H__
#define __TIMER_DRIVEN_SCHEDULING_AGENT_H__

#include "core/logging/Logger.h"
#include "core/Processor.h"
#include "core/ProcessContext.h"
#include "core/Repository.h"
#include "ThreadedSchedulingAgent.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
//  TimerDrivenSchedulingAgent Class
class TimerDrivenSchedulingAgent : public ThreadedSchedulingAgent {
 public:
  //  Constructor
  /*!
   * Create a new processor
   */
  TimerDrivenSchedulingAgent(std::shared_ptr<core::controller::ControllerServiceProvider> controller_service_provider, std::shared_ptr<core::Repository> repo,
                             std::shared_ptr<core::Repository> flow_repo, std::shared_ptr<core::ContentRepository> content_repo, std::shared_ptr<Configure> configure)
      : ThreadedSchedulingAgent(controller_service_provider, repo, flow_repo, content_repo, configure),
        logger_(logging::LoggerFactory<TimerDrivenSchedulingAgent>::getLogger()) {
  }
  //  Destructor
  virtual ~TimerDrivenSchedulingAgent() {
  }
  /**
   * Run function that accepts the processor, context and session factory.
   */
  uint64_t run(const std::shared_ptr<core::Processor> &processor, const std::shared_ptr<core::ProcessContext> &processContext, const std::shared_ptr<core::ProcessSessionFactory> &sessionFactory);

 private:
  // Prevent default copy constructor and assignment operation
  // Only support pass by reference or pointer
  TimerDrivenSchedulingAgent(const TimerDrivenSchedulingAgent &parent);
  TimerDrivenSchedulingAgent &operator=(const TimerDrivenSchedulingAgent &parent);

  std::shared_ptr<logging::Logger> logger_;
};

} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
#endif
