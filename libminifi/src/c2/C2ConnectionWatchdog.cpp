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
#include "c2/C2ConnectionWatchdog.h"

#include <condition_variable>
#include <mutex>
#include <stdexcept>

#include "c2/C2ExitCodes.h"

using namespace std::literals::chrono_literals;

namespace org::apache::nifi::minifi::c2 {

C2ConnectionWatchdog::C2ConnectionWatchdog(std::chrono::milliseconds connection_timeout, minifi::RequestShutdownCallback request_shutdown)
    : connection_timeout_(connection_timeout),
      request_shutdown_(std::move(request_shutdown)) {
  if (connection_timeout_ <= 0ms) {
    throw std::invalid_argument("C2 connection timeout must be greater than 0");
  }

  watchdog_thread_ = std::jthread([this](const std::stop_token& st) {
    std::condition_variable_any cv;
    std::mutex sleep_mutex;
    std::unique_lock<std::mutex> sleep_lock(sleep_mutex);
    while (!st.stop_requested()) {
      const auto now = std::chrono::steady_clock::now();
      const auto deadline = last_heartbeat_.load() + connection_timeout_;
      if (now >= deadline) {
        logger_->log_error("C2 connection timeout exceeded, shutting down...");
        request_shutdown_(minifi::ShutdownType::SHUTDOWN, C2_CONNECTION_TIMEOUT_EXIT_CODE);
        break;
      }
      cv.wait_until(sleep_lock, st, deadline, [] { return false; });
    }
  });
}

}  // namespace org::apache::nifi::minifi::c2
