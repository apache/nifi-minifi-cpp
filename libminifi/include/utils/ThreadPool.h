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
#include "controllers/ThreadManagementService.h"
#include "core/controller/ControllerService.h"
#include "core/controller/ControllerServiceProvider.h"
namespace org::apache::nifi::minifi::utils {

using TaskId = std::string;

/**
 * Worker task
 * purpose: Provides a wrapper for the functor
 * and returns a future based on the template argument.
 */
template<typename T>
class Worker {
 public:
  explicit Worker(const std::function<T()> &task, const TaskId &identifier, std::unique_ptr<AfterExecute<T>> run_determinant)
      : identifier_(identifier),
        next_exec_time_(std::chrono::steady_clock::now()),
        task(task),
        run_determinant_(std::move(run_determinant)) {
    promise = std::make_shared<std::promise<T>>();
  }

  explicit Worker(const std::function<T()> &task, const TaskId &identifier)
      : identifier_(identifier),
        next_exec_time_(std::chrono::steady_clock::now()),
        task(task),
        run_determinant_(nullptr) {
    promise = std::make_shared<std::promise<T>>();
  }

  explicit Worker(const TaskId& identifier = {})
      : identifier_(identifier),
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
    T result = task();
    if (run_determinant_ == nullptr || (run_determinant_->isFinished(result) || run_determinant_->isCancelled(result))) {
      promise->set_value(result);
      return false;
    }
    next_exec_time_ = std::max(next_exec_time_ + run_determinant_->wait_time(), std::chrono::steady_clock::now());
    return true;
  }

  virtual void setIdentifier(const TaskId& identifier) {
    identifier_ = identifier;
  }

  virtual std::chrono::steady_clock::time_point getNextExecutionTime() const {
    return next_exec_time_;
  }

  virtual std::chrono::milliseconds getWaitTime() const {
    return run_determinant_->wait_time();
  }


  std::shared_ptr<std::promise<T>> getPromise() const;

  const TaskId &getIdentifier() const {
    return identifier_;
  }

 protected:
  TaskId identifier_;
  std::chrono::steady_clock::time_point next_exec_time_;
  std::function<T()> task;
  std::unique_ptr<AfterExecute<T>> run_determinant_;
  std::shared_ptr<std::promise<T>> promise;
};

template<typename T>
class DelayedTaskComparator {
 public:
  bool operator()(Worker<T> &a, Worker<T> &b) {
    return a.getNextExecutionTime() > b.getNextExecutionTime();
  }
};

template<typename T>
std::shared_ptr<std::promise<T>> Worker<T>::getPromise() const {
  return promise;
}

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
template<typename T>
class ThreadPool {
 public:
  ThreadPool(int max_worker_threads = 2, bool daemon_threads = false, core::controller::ControllerServiceProvider* controller_service_provider = nullptr,
             std::string name = "NamelessPool")
      : daemon_threads_(daemon_threads),
        thread_reduction_count_(0),
        max_worker_threads_(max_worker_threads),
        adjust_threads_(false),
        running_(false),
        controller_service_provider_(controller_service_provider),
        name_(std::move(name)) {
    current_workers_ = 0;
    thread_manager_ = nullptr;
  }

  ThreadPool(const ThreadPool<T> &other) = delete;
  ThreadPool<T>& operator=(const ThreadPool<T> &other) = delete;
  ThreadPool(ThreadPool<T> &&other) = delete;
  ThreadPool<T>& operator=(ThreadPool<T> &&other) = delete;

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
  void execute(Worker<T> &&task, std::future<T> &future);

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

  void setControllerServiceProvider(core::controller::ControllerServiceProvider* controller_service_provider) {
    std::lock_guard<std::recursive_mutex> lock(manager_mutex_);
    bool was_running = running_;
    if (was_running) {
      shutdown();
    }
    controller_service_provider_ = controller_service_provider;
    if (was_running)
      start();
  }

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
// determines if threads are detached
  bool daemon_threads_;
  std::atomic<int> thread_reduction_count_;
// max worker threads
  int max_worker_threads_;
// current worker tasks.
  std::atomic<int> current_workers_;
// thread queue
  std::vector<std::shared_ptr<WorkerThread>> thread_queue_;
// manager thread
  std::thread manager_thread_;
// the thread responsible for putting delayed tasks to the worker queue when they had to be put
  std::thread delayed_scheduler_thread_;
// conditional that's used to adjust the threads
  std::atomic<bool> adjust_threads_;
// atomic running boolean
  std::atomic<bool> running_;
// controller service provider
  core::controller::ControllerServiceProvider* controller_service_provider_;
// integrated power manager
  std::shared_ptr<controllers::ThreadManagementService> thread_manager_;
  // thread queue for the recently deceased threads.
  ConcurrentQueue<std::shared_ptr<WorkerThread>> deceased_thread_queue_;
// worker queue of worker objects
  ConditionConcurrentQueue<Worker<T>> worker_queue_;
  std::priority_queue<Worker<T>, std::vector<Worker<T>>, DelayedTaskComparator<T>> delayed_worker_queue_;
// mutex to  protect task status and delayed queue
  std::mutex worker_queue_mutex_;
// notification for new delayed tasks that's before the current ones
  std::condition_variable delayed_task_available_;
// map to identify if a task should be
  std::map<TaskId, bool> task_status_;
// manager mutex
  std::recursive_mutex manager_mutex_;
  // thread pool name
  std::string name_;
  // count of running tasks by ID
  std::unordered_map<TaskId, uint32_t> running_task_count_by_id_;
  // variable to signal task running completion
  std::condition_variable task_run_complete_;

  /**
   * Call for the manager to start worker threads
   */
  void manageWorkers();

  /**
   * Runs worker tasks
   */
  void run_tasks(std::shared_ptr<WorkerThread> thread);

  void manage_delayed_queue();
};

}  // namespace org::apache::nifi::minifi::utils
