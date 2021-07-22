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

#include <string>
#include <iostream>
#include <memory>
#include "utils/StringUtils.h"
#include "io/validation.h"
#include "core/controller/ControllerService.h"
#include "core/logging/LoggerConfiguration.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace controllers {

/**
 * Purpose: Thread management service provides a contextual awareness across
 * thread pools that enables us to deliver QOS to an agent.
 */
class ThreadManagementService : public core::controller::ControllerService {
 public:
  explicit ThreadManagementService(const std::string &name, const utils::Identifier &uuid = {})
      : ControllerService(name, uuid),
        logger_(logging::LoggerFactory<ThreadManagementService>::getLogger()) {
  }

  explicit ThreadManagementService(const std::string &name, const std::shared_ptr<Configure>& /*configuration*/)
      : ControllerService(name),
        logger_(logging::LoggerFactory<ThreadManagementService>::getLogger()) {
  }

  /**
   * Helps to determine if the number of tasks will increase the pools above their threshold.
   * @param new_tasks tasks to be added.
   * @return true if above max, false otherwise.
   */
  virtual bool isAboveMax(const int new_tasks) = 0;

  /**
   * Returns the max number of threads allowed by all pools
   * @return max threads.
   */
  virtual uint16_t getMaxThreads() = 0;

  /**
   * Function based on cooperative multitasking that will tell a caller whether or not the number of threads should be reduced.
   * @return true if threading impacts QOS.
   */
  virtual bool shouldReduce() = 0;

  /**
   * Function to indicate to this controller service that we've reduced threads in a threadpool
   */
  virtual void reduce() = 0;

  /**
   * Registration function to tabulate total threads.
   * @param threads threads from a thread pool.
   */
  virtual void registerThreadCount(const int threads) {
    thread_count_ += threads;
  }

  /**
   * Function to help callers identify if they can increase threads.
   * @return true if QOS won't be breached.
   */
  virtual bool canIncrease() = 0;

  virtual void initialize() {
    ControllerService::initialize();
  }

  void yield() {
  }

  bool isRunning() {
    return getState() == core::controller::ControllerServiceState::ENABLED;
  }

  bool isWorkAvailable() {
    return false;
  }

  virtual void onEnable() {
  }

 protected:
  std::atomic<int> thread_count_;

 private:
  std::shared_ptr<logging::Logger> logger_;
};

}  // namespace controllers
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org

#endif  // LIBMINIFI_INCLUDE_CONTROLLERS_THREADMANAGEMENTSERVICE_H_
