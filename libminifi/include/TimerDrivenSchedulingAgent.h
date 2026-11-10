// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#pragma once

#include <memory>

#include "minifi-cpp/core/logging/Logger.h"
#include "core/Processor.h"
#include "minifi-cpp/core/ProcessContext.h"
#include "minifi-cpp/core/Repository.h"
#include "ThreadedSchedulingAgent.h"

namespace org::apache::nifi::minifi {
class TimerDrivenSchedulingAgent : public ThreadedSchedulingAgent {
 public:
  TimerDrivenSchedulingAgent(const gsl::not_null<core::controller::ControllerServiceProvider*> controller_service_provider, std::shared_ptr<provenance::ProvenanceRepository> provenance_repo,
                             std::shared_ptr<core::Repository> flow_repo, std::shared_ptr<core::ContentRepository> content_repo, std::shared_ptr<Configure> configure,
                             utils::ThreadPool &thread_pool)
      : ThreadedSchedulingAgent(controller_service_provider, provenance_repo, flow_repo, content_repo, configure, thread_pool) {
  }

  utils::TaskRescheduleInfo run(core::Processor* processor, const std::shared_ptr<core::ProcessContext> &processContext,
      const std::shared_ptr<core::ProcessSessionFactory> &sessionFactory) override;

 private:
  std::shared_ptr<core::logging::Logger> logger_ = core::logging::LoggerFactory<TimerDrivenSchedulingAgent>::getLogger();
};

}  // namespace org::apache::nifi::minifi
