// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
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
  EventDrivenSchedulingAgent(const gsl::not_null<core::controller::ControllerServiceProvider*> controller_service_provider, std::shared_ptr<provenance::ProvenanceRepository> provenance_repo,
                             std::shared_ptr<core::Repository> flow_repo, std::shared_ptr<core::ContentRepository> content_repo, std::shared_ptr<Configure> configuration,
                             utils::ThreadPool &thread_pool)
      : ThreadedSchedulingAgent(controller_service_provider, provenance_repo, flow_repo, content_repo, configuration, thread_pool) {
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
