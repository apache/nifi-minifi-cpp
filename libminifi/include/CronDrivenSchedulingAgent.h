/**
 * @file CronDrivenSchedulingAgent.h
 * CronDrivenSchedulingAgent class declaration
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
#ifndef LIBMINIFI_INCLUDE_CRONDRIVENSCHEDULINGAGENT_H_
#define LIBMINIFI_INCLUDE_CRONDRIVENSCHEDULINGAGENT_H_

#include "core/logging/Logger.h"
#include "core/Processor.h"
#include "core/ProcessContext.h"
#include "core/ProcessSessionFactory.h"
#include "ThreadedSchedulingAgent.h"
#include <chrono>

#include "Cron.h"
namespace org {
namespace apache {
namespace nifi {
namespace minifi {

// CronDrivenSchedulingAgent Class
class CronDrivenSchedulingAgent : public ThreadedSchedulingAgent {
 public:
  // Constructor
  /*!
   * Create a new event driven scheduling agent.
   */
  CronDrivenSchedulingAgent(std::shared_ptr<core::controller::ControllerServiceProvider> controller_service_provider, std::shared_ptr<core::Repository> repo,
                            std::shared_ptr<core::Repository> flow_repo, std::shared_ptr<core::ContentRepository> content_repo, std::shared_ptr<Configure> configuration,
                            utils::ThreadPool<utils::TaskRescheduleInfo> &thread_pool)
      : ThreadedSchedulingAgent(controller_service_provider, repo, flow_repo, content_repo, configuration, thread_pool) {
  }
  // Destructor
  virtual ~CronDrivenSchedulingAgent() {
  }
  // Run function for the thread
  utils::TaskRescheduleInfo run(const std::shared_ptr<core::Processor> &processor, const std::shared_ptr<core::ProcessContext> &processContext,
      const std::shared_ptr<core::ProcessSessionFactory> &sessionFactory) override;

  void stop() override {
    std::lock_guard<std::mutex> locK(mutex_);
    schedules_.clear();
    last_exec_.clear();
  }

 private:
  std::mutex mutex_;
  std::map<std::string, Bosma::Cron> schedules_;
  std::map<std::string, std::chrono::system_clock::time_point> last_exec_;
  // Prevent default copy constructor and assignment operation
  // Only support pass by reference or pointer
  CronDrivenSchedulingAgent(const CronDrivenSchedulingAgent &parent);
  CronDrivenSchedulingAgent &operator=(const CronDrivenSchedulingAgent &parent);

};

} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
#endif
