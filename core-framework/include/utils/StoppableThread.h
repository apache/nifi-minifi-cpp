// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#pragma once

#include <mutex>
#include <thread>
#include <condition_variable>
#include <atomic>
#include <functional>

namespace org::apache::nifi::minifi::utils {

// mimics some aspects of std::jthread
// unfortunately clang's jthread support is lacking
// TODO(adebreceni): replace this with std::jthread
class StoppableThread {
 public:
  explicit StoppableThread(std::function<void()> fn);

  void stopAndJoin() {
    running_ = false;
    {
      std::unique_lock lock(mtx_);
      cv_.notify_all();
    }
    if (thread_.joinable()) {
      thread_.join();
    }
  }

  ~StoppableThread() {
    stopAndJoin();
  }

  // return true if stop was requested
  static bool waitForStopRequest(std::chrono::milliseconds time);

 private:
  std::atomic_bool running_{true};
  std::mutex mtx_;
  std::condition_variable cv_;
  std::thread thread_;
};

}  // namespace org::apache::nifi::minifi::utils
