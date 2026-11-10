// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#include "core/state/ProcessorController.h"
#include <memory>
#include <utility>

namespace org::apache::nifi::minifi::state {

ProcessorController::ProcessorController(core::Processor& processor, SchedulingAgent& scheduler)
    : processor_(&processor),
      scheduler_(&scheduler) {
}

ProcessorController::~ProcessorController() = default;
/**
 * Start the client
 */
int16_t ProcessorController::start() {
  processor_->setScheduledState(core::ScheduledState::RUNNING);
  scheduler_->schedule(processor_);
  return 0;
}
/**
 * Stop the client
 */
int16_t ProcessorController::stop() {
  scheduler_->unschedule(processor_);
  return 0;
}

bool ProcessorController::isRunning() const {
  return processor_->isRunning();
}

int16_t ProcessorController::pause() {
  return stop();
}

int16_t ProcessorController::resume() {
  return start();
}

}  // namespace org::apache::nifi::minifi::state
