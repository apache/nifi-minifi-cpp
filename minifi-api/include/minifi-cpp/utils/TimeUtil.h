// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#pragma once

#include <chrono>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <string>

namespace org::apache::nifi::minifi::utils::timeutils {

class Clock {
 public:
  virtual ~Clock() = default;
  Clock() = default;
  Clock(const Clock&) = delete;
  Clock(Clock&&) = delete;
  Clock& operator=(const Clock&) = delete;
  Clock& operator=(Clock&&) = delete;

  [[nodiscard]] virtual std::chrono::milliseconds timeSinceEpoch() const = 0;
  virtual bool wait_until(std::condition_variable& cv, std::unique_lock<std::mutex>& lck, std::chrono::milliseconds time,
      const std::function<bool()>& pred) {
    return cv.wait_for(lck, time - timeSinceEpoch(), pred);
  }
};

class SteadyClock : public Clock {
 public:
  [[nodiscard]] std::chrono::milliseconds timeSinceEpoch() const override {
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch());
  }

  [[nodiscard]] virtual std::chrono::time_point<std::chrono::steady_clock> now() const { return std::chrono::steady_clock::now(); }
};

std::shared_ptr<SteadyClock> getClock();

// test-only utility to specify what clock to use
void setClock(std::shared_ptr<SteadyClock> clock);

#ifdef WIN32
void dateSetGlobalInstall(const std::string& install);
#endif

}  // namespace org::apache::nifi::minifi::utils::timeutils
