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

#include <atomic>
#include <chrono>
#include <memory>
#include <thread>

#include "RequestShutdown.h"
#include "core/logging/LoggerFactory.h"
#include "minifi-cpp/core/logging/Logger.h"

namespace org::apache::nifi::minifi::c2 {

class C2ConnectionWatchdog {
 public:
  C2ConnectionWatchdog(std::chrono::milliseconds connection_timeout, minifi::RequestShutdownCallback request_shutdown);

  void heartBeatHappened() {
    last_heartbeat_ = std::chrono::steady_clock::now();
  }

  void stop() {
    watchdog_thread_.request_stop();
  }

 private:
  std::chrono::milliseconds connection_timeout_;
  std::atomic<std::chrono::steady_clock::time_point> last_heartbeat_ = std::chrono::steady_clock::now();
  minifi::RequestShutdownCallback request_shutdown_;
  std::shared_ptr<core::logging::Logger> logger_ = core::logging::LoggerFactory<C2ConnectionWatchdog>::getLogger();
  std::jthread watchdog_thread_;
};

}  // namespace org::apache::nifi::minifi::c2
