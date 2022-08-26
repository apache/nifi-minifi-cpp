/**
 * @file ThreadedSchedulingAgent.h
 * ThreadedSchedulingAgent class declaration
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
#ifndef LIBMINIFI_INCLUDE_THREADEDSCHEDULINGAGENT_H_
#define LIBMINIFI_INCLUDE_THREADEDSCHEDULINGAGENT_H_

#include <memory>
#include <set>
#include <string>
#include <chrono>
#include "properties/Configure.h"
#include "core/logging/LoggerFactory.h"
#include "core/Processor.h"
#include "core/Repository.h"
#include "core/ProcessContext.h"
#include "SchedulingAgent.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {

/**
 * An abstract scheduling agent which creates and manages a pool of threads for
 * each processor scheduled.
 */
class ThreadedSchedulingAgent : public SchedulingAgent {
 public:
  // Constructor
  /*!
   * Create a new threaded scheduling agent.
   */
  ThreadedSchedulingAgent(const gsl::not_null<core::controller::ControllerServiceProvider*> controller_service_provider, std::shared_ptr<core::Repository> repo,
        std::shared_ptr<core::Repository> flow_repo, std::shared_ptr<core::ContentRepository> content_repo,
        std::shared_ptr<Configure> configuration,  utils::ThreadPool<utils::TaskRescheduleInfo> &thread_pool)
      : SchedulingAgent(controller_service_provider, repo, flow_repo, content_repo, configuration, thread_pool) {
  }
  // Destructor
  ~ThreadedSchedulingAgent() override = default;

  // Run function for the thread
  virtual utils::TaskRescheduleInfo run(core::Processor* processor, const std::shared_ptr<core::ProcessContext> &processContext,
                       const std::shared_ptr<core::ProcessSessionFactory> &sessionFactory) = 0;

 public:
  // schedule, overwritten by different DrivenTimerDrivenSchedulingAgent
  void schedule(core::Processor* processor) override;
  // unschedule, overwritten by different DrivenTimerDrivenSchedulingAgent
  void unschedule(core::Processor* processor) override;

  void stop() override;

 private:
  // Prevent default copy constructor and assignment operation
  // Only support pass by reference or pointer
  ThreadedSchedulingAgent(const ThreadedSchedulingAgent &parent);
  ThreadedSchedulingAgent &operator=(const ThreadedSchedulingAgent &parent);
  std::shared_ptr<core::logging::Logger> logger_ = core::logging::LoggerFactory<ThreadedSchedulingAgent>::getLogger();

  std::set<utils::Identifier> processors_running_;  // Set just for easy usage
};

}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
#endif  // LIBMINIFI_INCLUDE_THREADEDSCHEDULINGAGENT_H_
