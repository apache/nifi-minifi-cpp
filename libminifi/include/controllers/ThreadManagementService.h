/**
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
#ifndef LIBMINIFI_INCLUDE_CONTROLLERS_THREADMANAGEMENTSERVICE_H_
#define LIBMINIFI_INCLUDE_CONTROLLERS_THREADMANAGEMENTSERVICE_H_

#include <iostream>
#include <string>
#include <memory>
#include <utility>

#include "utils/StringUtils.h"
#include "io/validation.h"
#include "core/controller/ControllerService.h"
#include "core/logging/LoggerFactory.h"
#include "minifi-cpp/controllers/ThreadManagementService.h"

namespace org::apache::nifi::minifi::controllers {

/**
 * Purpose: Thread management service provides a contextual awareness across
 * thread pools that enables us to deliver QOS to an agent.
 */
class ThreadManagementServiceImpl : public core::controller::ControllerServiceImpl, public virtual ThreadManagementService {
 public:
  explicit ThreadManagementServiceImpl(std::string_view name, const utils::Identifier &uuid = {})
      : ControllerServiceImpl(name, uuid),
        logger_(core::logging::LoggerFactory<ThreadManagementService>::getLogger()) {
  }

  explicit ThreadManagementServiceImpl(std::string_view name, const std::shared_ptr<Configure>& /*configuration*/)
      : ControllerServiceImpl(name),
        logger_(core::logging::LoggerFactory<ThreadManagementService>::getLogger()) {
  }

  /**
   * Registration function to tabulate total threads.
   * @param threads threads from a thread pool.
   */
  void registerThreadCount(const int threads) override {
    thread_count_ += threads;
  }

  void initialize() override {
    ControllerServiceImpl::initialize();
  }

  void yield() override {
  }

  bool isRunning() const override {
    return getState() == core::controller::ControllerServiceState::ENABLED;
  }

  bool isWorkAvailable() override {
    return false;
  }

  void onEnable() override {
  }

 protected:
  std::atomic<int> thread_count_;

 private:
  std::shared_ptr<core::logging::Logger> logger_;
};

}  // namespace org::apache::nifi::minifi::controllers

#endif  // LIBMINIFI_INCLUDE_CONTROLLERS_THREADMANAGEMENTSERVICE_H_
