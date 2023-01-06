/**
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
