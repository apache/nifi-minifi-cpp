/**
 * @file EventDrivenSchedulingAgent.h
 * EventDrivenSchedulingAgent class declaration
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
#ifndef LIBMINIFI_INCLUDE_EVENTDRIVENSCHEDULINGAGENT_H_
#define LIBMINIFI_INCLUDE_EVENTDRIVENSCHEDULINGAGENT_H_

#include <memory>
#include <string>

#define DEFAULT_TIME_SLICE_MS 500

#include "core/logging/Logger.h"
#include "core/Processor.h"
#include "core/ProcessContext.h"
#include "core/ProcessSessionFactory.h"
#include "ThreadedSchedulingAgent.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {

// EventDrivenSchedulingAgent Class
class EventDrivenSchedulingAgent : public ThreadedSchedulingAgent {
 public:
  // Constructor
  /*!
   * Create a new event driven scheduling agent.
   */
  EventDrivenSchedulingAgent(const gsl::not_null<core::controller::ControllerServiceProvider*> controller_service_provider, std::shared_ptr<core::Repository> repo,
                             std::shared_ptr<core::Repository> flow_repo, std::shared_ptr<core::ContentRepository> content_repo, std::shared_ptr<Configure> configuration,
                             utils::ThreadPool<utils::TaskRescheduleInfo> &thread_pool)
      : ThreadedSchedulingAgent(controller_service_provider, repo, flow_repo, content_repo, configuration, thread_pool) {
    int slice = configuration->getInt(Configure::nifi_flow_engine_event_driven_time_slice, DEFAULT_TIME_SLICE_MS);
    if (slice < 10 || 1000 < slice) {
      throw Exception(FLOW_EXCEPTION, std::string(Configure::nifi_flow_engine_event_driven_time_slice) + " is out of reasonable range!");
    }
    time_slice_ = std::chrono::milliseconds(slice);
  }

  void schedule(core::Processor* processor) override;

  // Run function for the thread
  utils::TaskRescheduleInfo run(core::Processor* processor, const std::shared_ptr<core::ProcessContext> &processContext,
      const std::shared_ptr<core::ProcessSessionFactory> &sessionFactory) override;

 private:
  // Prevent default copy constructor and assignment operation
  // Only support pass by reference or pointer
  EventDrivenSchedulingAgent(const EventDrivenSchedulingAgent &parent);
  EventDrivenSchedulingAgent &operator=(const EventDrivenSchedulingAgent &parent);

  std::chrono::milliseconds time_slice_;
};

}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
#endif  // LIBMINIFI_INCLUDE_EVENTDRIVENSCHEDULINGAGENT_H_
