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

#include <algorithm>
#include <atomic>
#include <chrono>
#include <functional>
#include <future>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <queue>
#include <sstream>
#include <string>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

#include "BackTrace.h"
#include "MinifiConcurrentQueue.h"
#include "Monitors.h"
#include "core/expect.h"
#include "minifi-cpp/controllers/ThreadManagementService.h"
#include "minifi-cpp/core/controller/ControllerServiceLookup.h"

namespace org::apache::nifi::minifi::utils {

using TaskId = std::string;

/**
 * Worker task
 * purpose: Provides a wrapper for the functor
 * and returns a future based on the template argument.
 */
class Worker {
 public:
  explicit Worker(const std::function<TaskRescheduleInfo()> &task, TaskId identifier)
      : identifier_(std::move(identifier)),
        next_exec_time_(std::chrono::steady_clock::now()),
        task(task) {
    promise = std::make_shared<std::promise<TaskRescheduleInfo>>();
  }

  explicit Worker(TaskId  identifier = {})
      : identifier_(std::move(identifier)),
        next_exec_time_(std::chrono::steady_clock::now()) {
  }

  virtual ~Worker() = default;

  Worker(const Worker&) = delete;
  Worker(Worker&&) noexcept = default;
  Worker& operator=(const Worker&) = delete;
  Worker& operator=(Worker&&) noexcept = default;

  /**
   * Runs the task and takes the output from the functor
   * setting the result into the promise
   * @return whether or not to continue running
   *   false == finished || error
   *   true == run again
   */
  virtual bool run() {
    TaskRescheduleInfo result = task();
    if (result.isFinished()) {
      promise->set_value(result);
      return false;
    }

    next_exec_time_ = result.getNextExecutionTime();
    return true;
  }

  [[nodiscard]] virtual std::chrono::steady_clock::time_point getNextExecutionTime() const {
    return next_exec_time_;
  }

  [[nodiscard]] std::shared_ptr<std::promise<TaskRescheduleInfo>> getPromise() const { return promise; }

  [[nodiscard]] const TaskId &getIdentifier() const {
    return identifier_;
  }

 protected:
  TaskId identifier_;
  std::chrono::steady_clock::time_point next_exec_time_;
  std::function<TaskRescheduleInfo()> task;
  std::shared_ptr<std::promise<TaskRescheduleInfo>> promise;
};

class DelayedTaskComparator {
 public:
  bool operator()(Worker &a, Worker &b) {
    return a.getNextExecutionTime() > b.getNextExecutionTime();
  }
};

class WorkerThread {
 public:
  explicit WorkerThread(std::thread thread, const std::string &name = "NamelessWorker")
      : is_running_(false),
        thread_(std::move(thread)),
        name_(name) {
  }
  WorkerThread(const std::string &name = "NamelessWorker") // NOLINT
      : is_running_(false),
        name_(name) {
  }
  std::atomic<bool> is_running_;
  std::thread thread_;
  std::string name_;
};

/**
 * Thread pool
 * Purpose: Provides a thread pool with basic functionality similar to
 * ThreadPoolExecutor
 * Design: Locked control over a manager thread that controls the worker threads
 */
class ThreadPool {
 public:
  ThreadPool(int max_worker_threads = 2,
             core::controller::ControllerServiceLookup* controller_service_provider = nullptr, std::string name = "NamelessPool");

  ThreadPool(const ThreadPool &other) = delete;
  ThreadPool& operator=(const ThreadPool &other) = delete;
  ThreadPool(ThreadPool &&other) = delete;
  ThreadPool& operator=(ThreadPool &&other) = delete;

  ~ThreadPool() {
    shutdown();
  }

  /**
   * Execute accepts a worker task and returns
   * a future
   * @param task this thread pool will subsume ownership of
   * the worker task
   * @param future future to move new promise to
   */
  void execute(Worker &&task, std::future<TaskRescheduleInfo> &future);

  /**
   * attempts to stop tasks with the provided identifier.
   * @param identifier for worker tasks. Note that these tasks won't
   * immediately stop.
   */
  void stopTasks(const TaskId &identifier);

  /**
   * resumes work queue processing.
   */
  void resume();

  /**
   * pauses work queue processing
   */
  void pause();

  /**
   * Returns true if a task is running.
   */
  bool isTaskRunning(const TaskId &identifier) {
    std::unique_lock<std::mutex> lock(worker_queue_mutex_);
    const auto iter = task_status_.find(identifier);
    if (iter == task_status_.end())
      return false;
    return iter->second;
  }

  bool isRunning() const {
    return running_.load();
  }

  std::vector<BackTrace> getTraces() {
    std::vector<BackTrace> traces;
    std::lock_guard<std::recursive_mutex> lock(manager_mutex_);
    std::unique_lock<std::mutex> wlock(worker_queue_mutex_);
    // while we may be checking if running, we don't want to
    // use the threads outside of the manager mutex's lock -- therefore we will
    // obtain a lock so we can keep the threads in memory
    if (running_) {
      for (const auto &worker : thread_queue_) {
        if (worker->is_running_)
          traces.emplace_back(TraceResolver::getResolver().getBackTrace(worker->name_, worker->thread_.native_handle()));
      }
    }
    return traces;
  }

  /**
   * Starts the Thread Pool
   */
  void start();
  /**
   * Shutdown the thread pool and clear any
   * currently running activities
   */
  void shutdown();
  /**
   * Set the max concurrent tasks. When this is done
   * we must start and restart the thread pool if
   * the number of tasks is less than the currently configured number
   */
  void setMaxConcurrentTasks(uint16_t max) {
    std::lock_guard<std::recursive_mutex> lock(manager_mutex_);
    bool was_running = running_;
    if (was_running) {
      shutdown();
    }
    max_worker_threads_ = max;
    if (was_running)
      start();
  }

  void setControllerServiceProvider(core::controller::ControllerServiceLookup* controller_service_provider) {
    std::lock_guard<std::recursive_mutex> lock(manager_mutex_);
    bool was_running = running_;
    if (was_running) {
      shutdown();
    }
    controller_service_provider_ = controller_service_provider;
    if (was_running)
      start();
  }

 private:
  std::shared_ptr<controllers::ThreadManagementService> createThreadManager() const;

 protected:
  std::thread createThread(std::function<void()> &&functor) {
    return std::thread([ functor ]() mutable {
      functor();
    });
  }

  /**
   * Drain will notify tasks to stop following notification
   */
  void drain() {
    worker_queue_.stop();
    while (current_workers_ > 0) {
      // The sleeping workers were waken up and stopped, but we have to wait
      // the ones that actually worked on something when the queue was stopped.
      // Stopping the queue guarantees that they don't get any new task.
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
  }

  std::atomic<int> thread_reduction_count_;
  int max_worker_threads_;
  std::atomic<int> current_workers_;
  std::vector<std::shared_ptr<WorkerThread>> thread_queue_;
  std::thread manager_thread_;
  std::thread delayed_scheduler_thread_;
  std::atomic<bool> adjust_threads_;
  std::atomic<bool> running_;
  core::controller::ControllerServiceLookup* controller_service_provider_;
  std::shared_ptr<controllers::ThreadManagementService> thread_manager_;
  ConcurrentQueue<std::shared_ptr<WorkerThread>> deceased_thread_queue_;
  ConditionConcurrentQueue<Worker> worker_queue_;
  std::priority_queue<Worker, std::vector<Worker>, DelayedTaskComparator> delayed_worker_queue_;
  std::mutex worker_queue_mutex_;
  std::condition_variable delayed_task_available_;
  std::map<TaskId, bool> task_status_;
  std::recursive_mutex manager_mutex_;
  std::string name_;
  std::unordered_map<TaskId, uint32_t> running_task_count_by_id_;
  std::condition_variable task_run_complete_;

  std::shared_ptr<core::logging::Logger> logger_;


  void manageWorkers();
  void run_tasks(const std::shared_ptr<WorkerThread>& thread);
  void manage_delayed_queue();
};

}  // namespace org::apache::nifi::minifi::utils
