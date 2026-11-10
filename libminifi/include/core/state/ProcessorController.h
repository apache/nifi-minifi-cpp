// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#pragma once

#include <string>
#include <memory>
#include "core/Processor.h"
#include "SchedulingAgent.h"
#include "UpdateController.h"

namespace org::apache::nifi::minifi::state {

/**
 * Purpose, Justification, & Design: ProcessController is the state control mechanism for processors.
 * This is coupled with the scheduler. Since scheduling agents run processors, we must ensure state
 * is set in the processor to enable it to run, after which the scheduling agent will then be allowed
 * to run the aforementioned processor.
 */
class ProcessorController : public StateController {
 public:
  ProcessorController(core::Processor& processor, SchedulingAgent& scheduler);

  ~ProcessorController() override;

  [[nodiscard]] std::string getComponentName() const override {
    return processor_->getName();
  }

  [[nodiscard]] utils::Identifier getComponentUUID() const override {
    return processor_->getUUID();
  }

  core::Processor& getProcessor() {
    return *processor_;
  }
  /**
   * Start the client
   */
  int16_t start() override;
  /**
   * Stop the client
   */
  int16_t stop() override;

  bool isRunning() const override;

  int16_t pause() override;

  int16_t resume() override;

 protected:
  gsl::not_null<core::Processor*> processor_;
  gsl::not_null<SchedulingAgent*> scheduler_;
};

}  // namespace org::apache::nifi::minifi::state
