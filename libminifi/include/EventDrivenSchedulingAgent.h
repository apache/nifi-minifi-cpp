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
#pragma once

#include <memory>
#include <string>
#include <chrono>

constexpr auto DEFAULT_TIME_SLICE = std::chrono::milliseconds(500);

#include "minifi-cpp/core/logging/Logger.h"
#include "core/Processor.h"
#include "minifi-cpp/core/ProcessContext.h"
#include "minifi-cpp/core/ProcessSessionFactory.h"
#include "ThreadedSchedulingAgent.h"

namespace org::apache::nifi::minifi {

class EventDrivenSchedulingAgent : public ThreadedSchedulingAgent {
 public:
  EventDrivenSchedulingAgent(const gsl::not_null<core::controller::ControllerServiceProvider*> controller_service_provider, std::shared_ptr<core::Repository> repo,
                             std::shared_ptr<core::Repository> flow_repo, std::shared_ptr<core::ContentRepository> content_repo, std::shared_ptr<Configure> configuration,
                             utils::ThreadPool &thread_pool)
      : ThreadedSchedulingAgent(controller_service_provider, repo, flow_repo, content_repo, configuration, thread_pool) {
    using namespace std::literals::chrono_literals;

    time_slice_ = configuration->get(Configure::nifi_flow_engine_event_driven_time_slice)
        | utils::andThen(utils::timeutils::StringToDuration<std::chrono::milliseconds>)
        | utils::valueOrElse([] { return DEFAULT_TIME_SLICE; });

    if (time_slice_ < 10ms || 1000ms < time_slice_) {
      throw Exception(FLOW_EXCEPTION, std::string(Configure::nifi_flow_engine_event_driven_time_slice) + " is out of reasonable range!");
    }
  }

  void schedule(core::Processor* processor) override;

  utils::TaskRescheduleInfo run(core::Processor* processor, const std::shared_ptr<core::ProcessContext> &process_context,
      const std::shared_ptr<core::ProcessSessionFactory> &session_factory) override;

 private:
  std::chrono::milliseconds time_slice_{};
};

}  // namespace org::apache::nifi::minifi
