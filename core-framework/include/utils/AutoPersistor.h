// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#pragma once

#include <condition_variable>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <utility>

#include "core/ConfigurableComponentImpl.h"
#include "core/Core.h"
#include "core/logging/LoggerFactory.h"
#include "minifi-cpp/utils/Export.h"

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
