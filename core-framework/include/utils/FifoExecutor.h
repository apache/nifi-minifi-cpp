// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#pragma once

#include <future>
#include <thread>
#include <utility>

#include "utils/MinifiConcurrentQueue.h"

namespace org::apache::nifi::minifi::utils {

namespace detail {
class WorkerThread final {
 public:
  WorkerThread();

  WorkerThread(const WorkerThread&) = delete;
  WorkerThread(WorkerThread&&) = delete;
  WorkerThread& operator=(WorkerThread) = delete;

  ~WorkerThread();

  template<typename... Args>
  void enqueue(Args&&... args) { task_queue_.enqueue(std::forward<Args>(args)...); }

 private:
  void run() noexcept;

  utils::ConditionConcurrentQueue<std::packaged_task<void()>> task_queue_;
  std::thread thread_;
};
}  // namespace detail

/**
 * Executes arbitrary functions with no parameters asynchronously on an internal thread, returning a future to the result.
 */
class FifoExecutor final {
 public:
  template<typename Func>
  auto enqueue(Func func) -> std::future<decltype(func())> {
    using result_type = decltype(func());
    std::packaged_task<result_type()> task{std::move(func)};
    auto future = task.get_future();
    worker_thread_.enqueue(std::move(task));
    return future;
  }
 private:
  detail::WorkerThread worker_thread_;
};

}  // namespace org::apache::nifi::minifi::utils
