// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#include "TimerDrivenSchedulingAgent.h"
#include <chrono>
#include <memory>

using namespace std::literals::chrono_literals;

namespace org::apache::nifi::minifi {

utils::TaskRescheduleInfo TimerDrivenSchedulingAgent::run(core::Processor* processor, const std::shared_ptr<core::ProcessContext> &processContext,
                                         const std::shared_ptr<core::ProcessSessionFactory> &sessionFactory) {
  if (running_ && processor->isRunning()) {
    const auto trigger_start_time = std::chrono::steady_clock::now();
    auto result = triggerAndCommit(processor, processContext, sessionFactory);
    if (!result) {
      logger_->log_warn("Trigger and commit failed for processor {}", processor->getName());
    }

    const auto next_scheduled_run = trigger_start_time + processor->getSchedulingPeriod();
    const auto yield_expiration_time = processor->getYieldExpirationTime();
    return utils::TaskRescheduleInfo::RetryAfter(std::max(next_scheduled_run, yield_expiration_time));
  }
  return utils::TaskRescheduleInfo::Done();
}

}  // namespace org::apache::nifi::minifi
