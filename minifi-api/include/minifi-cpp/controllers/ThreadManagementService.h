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

#pragma once

#include <iostream>
#include <string>
#include <memory>
#include <utility>

#include "utils/StringUtils.h"
#include "io/validation.h"
#include "minifi-cpp/core/controller/ControllerService.h"

namespace org::apache::nifi::minifi::controllers {

/**
 * Purpose: Thread management service provides a contextual awareness across
 * thread pools that enables us to deliver QOS to an agent.
 */
class ThreadManagementService : public virtual core::controller::ControllerService {
 public:
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
  virtual void registerThreadCount(const int threads) = 0;

  /**
   * Function to help callers identify if they can increase threads.
   * @return true if QOS won't be breached.
   */
  virtual bool canIncrease() = 0;
};

}  // namespace org::apache::nifi::minifi::controllers
