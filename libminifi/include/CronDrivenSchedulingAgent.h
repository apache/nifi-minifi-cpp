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
#pragma once

#include <chrono>
#include <map>
#include <memory>
#include <string>
#include <utility>

#include "minifi-cpp/core/logging/Logger.h"
#include "minifi-cpp/core/ProcessContext.h"
#include "core/Processor.h"
#include "minifi-cpp/core/ProcessSessionFactory.h"
#include "utils/Cron.h"
#include "ThreadedSchedulingAgent.h"

namespace org::apache::nifi::minifi {

class CronDrivenSchedulingAgent : public ThreadedSchedulingAgent {
 public:
  CronDrivenSchedulingAgent(const gsl::not_null<core::controller::ControllerServiceProvider*> controller_service_provider,
                            std::shared_ptr<core::Repository> repo,
                            std::shared_ptr<core::Repository> flow_repo,
                            std::shared_ptr<core::ContentRepository> content_repo,
                            std::shared_ptr<Configure> configuration,
                            utils::ThreadPool& thread_pool)
      : ThreadedSchedulingAgent(controller_service_provider, std::move(repo), std::move(flow_repo), std::move(content_repo), std::move(configuration), thread_pool) {
  }

  ~CronDrivenSchedulingAgent() override = default;

  utils::TaskRescheduleInfo run(core::Processor *processor,
                                const std::shared_ptr<core::ProcessContext>& processContext,
                                const std::shared_ptr<core::ProcessSessionFactory>& sessionFactory) override;

  void stop() override {
    std::lock_guard<std::mutex> locK(mutex_);
    schedules_.clear();
    last_exec_.clear();
  }

 private:
  std::mutex mutex_;
  std::map<utils::Identifier, utils::Cron> schedules_;
  std::map<utils::Identifier, date::local_time<std::chrono::seconds>> last_exec_;
};

}  // namespace org::apache::nifi::minifi
