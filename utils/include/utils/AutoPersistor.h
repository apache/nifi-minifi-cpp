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

#include <condition_variable>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <utility>

#include "core/ConfigurableComponent.h"
#include "core/Core.h"
#include "core/logging/LoggerFactory.h"
#include "utils/Export.h"

namespace org::apache::nifi::minifi::controllers {

/**
 *  Persists in given intervals.
 *  Has an own thread, so stop() must be called before destruction of data used by persist_.
 */
class AutoPersistor {
 public:
  ~AutoPersistor();

  void start(bool always_persist, std::chrono::milliseconds auto_persistence_interval, std::function<bool()> persist);
  void stop();

  [[nodiscard]] bool isAlwaysPersisting() const {
    return always_persist_;
  }

 private:
  void persistingThreadFunc();

  bool always_persist_ = false;
  std::chrono::milliseconds auto_persistence_interval_{0};
  std::thread persisting_thread_;
  bool running_ = false;
  std::mutex persisting_mutex_;
  std::condition_variable persisting_cv_;
  std::function<bool()> persist_;
  std::shared_ptr<core::logging::Logger> logger_ = core::logging::LoggerFactory<AutoPersistor>::getLogger();
};

}  // namespace org::apache::nifi::minifi::controllers
