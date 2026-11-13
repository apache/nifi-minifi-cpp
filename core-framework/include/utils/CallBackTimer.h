// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements
#pragma once

#include <mutex>
#include <condition_variable>
#include <thread>
#include <chrono>
#include <functional>

namespace org::apache::nifi::minifi::utils {

class CallBackTimer {
 public:
  CallBackTimer(std::chrono::milliseconds interval, const std::function<void(void)>& func);
  ~CallBackTimer();

  void stop();

  void start();

  bool is_running() const;

 private:
  bool execute_;
  std::function<void(void)> func_;
  std::thread thd_;
  mutable std::mutex mtx_;
  mutable std::mutex cv_mtx_;
  std::condition_variable cv_;

  const std::chrono::milliseconds interval_;
};

}  // namespace org::apache::nifi::minifi::utils
