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
#ifndef LIBMINIFI_INCLUDE_THREAD_POOL_H
#define LIBMINIFI_INCLUDE_THREAD_POOL_H

#include <chrono>
#include <iostream>
#include <atomic>
#include <mutex>
#include <map>
#include <vector>
#include <queue>
#include <future>
#include <thread>
#include "concurrentqueue.h"
namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace utils {

/**
 * Worker task helper that determines
 * whether or not we will run
 */
template<typename T>
class AfterExecute {
 public:
  virtual ~AfterExecute() {

  }

  explicit AfterExecute() {

  }

  explicit AfterExecute(AfterExecute &&other) {

  }
  virtual bool isFinished(const T &result) = 0;
  virtual bool isCancelled(const T &result) = 0;
  /**
   * Time to wait before re-running this task if necessary
   * @return milliseconds since epoch after which we are eligible to re-run this task.
   */
  virtual int64_t wait_time() = 0;
};

/**
 * Worker task
 * purpose: Provides a wrapper for the functor
 * and returns a future based on the template argument.
 */
template<typename T>
class Worker {
 public:
  explicit Worker(std::function<T()> &task, const std::string &identifier, std::unique_ptr<AfterExecute<T>> run_determinant)
      : task(task),
        run_determinant_(std::move(run_determinant)),
        identifier_(identifier),
        time_slice_(0) {
    promise = std::make_shared<std::promise<T>>();
  }

  explicit Worker(std::function<T()> &task, const std::string &identifier)
      : task(task),
        run_determinant_(nullptr),
        identifier_(identifier),
        time_slice_(0) {
    promise = std::make_shared<std::promise<T>>();
  }

  explicit Worker(const std::string identifier = "")
      : identifier_(identifier),
        time_slice_(0) {
  }

  virtual ~Worker() {

  }

  /**
   * Move constructor for worker tasks
   */
  Worker(Worker &&other)
      : task(std::move(other.task)),
        promise(other.promise),
        time_slice_(std::move(other.time_slice_)),
        identifier_(std::move(other.identifier_)),
        run_determinant_(std::move(other.run_determinant_)) {
  }

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
    time_slice_ = increment_time(run_determinant_->wait_time());
    return true;
  }

  virtual void setIdentifier(const std::string identifier) {
    identifier_ = identifier;
  }

  virtual uint64_t getTimeSlice() {
    return time_slice_;
  }

  virtual uint64_t getWaitTime() {
    return run_determinant_->wait_time();
  }

  Worker<T>(const Worker<T>&) = delete;
  Worker<T>& operator =(const Worker<T>&) = delete;

  Worker<T>& operator =(Worker<T> &&);

  std::shared_ptr<std::promise<T>> getPromise();

  const std::string &getIdentifier() {
    return identifier_;
  }
 protected:

  inline uint64_t increment_time(const uint64_t &time) {
    std::chrono::time_point<std::chrono::system_clock> now = std::chrono::system_clock::now();
    auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
    return millis + time;
  }

  std::string identifier_;
  uint64_t time_slice_;
  std::function<T()> task;
  std::unique_ptr<AfterExecute<T>> run_determinant_;
  std::shared_ptr<std::promise<T>> promise;
};

template<typename T>
class WorkerComparator {
 public:
  bool operator()(Worker<T> &a, Worker<T> &b) {
    return a.getTimeSlice() < b.getTimeSlice();
  }
};

template<typename T>
Worker<T>& Worker<T>::operator =(Worker<T> && other) {
  task = std::move(other.task);
  promise = other.promise;
  time_slice_ = std::move(other.time_slice_);
  identifier_ = std::move(other.identifier_);
  run_determinant_ = std::move(other.run_determinant_);
  return *this;
}

template<typename T>
std::shared_ptr<std::promise<T>> Worker<T>::getPromise() {
  return promise;
}

/**
 * Thread pool
 * Purpose: Provides a thread pool with basic functionality similar to
 * ThreadPoolExecutor
 * Design: Locked control over a manager thread that controls the worker threads
 */
template<typename T>
class ThreadPool {
 public:

  ThreadPool(int max_worker_threads = 2, bool daemon_threads = false)
      : max_worker_threads_(max_worker_threads),
        daemon_threads_(daemon_threads),
        running_(false) {
    current_workers_ = 0;
  }

  ThreadPool(const ThreadPool<T> &&other)
      : max_worker_threads_(std::move(other.max_worker_threads_)),
        daemon_threads_(std::move(other.daemon_threads_)),
        running_(false) {
    current_workers_ = 0;
  }
  virtual ~ThreadPool() {
    shutdown();
  }

  /**
   * Execute accepts a worker task and returns
   * a future
   * @param task this thread pool will subsume ownership of
   * the worker task
   * @param future future to move new promise to
   * @return true if future can be created and thread pool is in a running state.
   */
  bool execute(Worker<T> &&task, std::future<T> &future);

  /**
   * attempts to stop tasks with the provided identifier.
   * @param identifier for worker tasks. Note that these tasks won't
   * immediately stop.
   */
  void stopTasks(const std::string &identifier);

  /**
   * Returns true if a task is running.
   */
  bool isRunning(const std::string &identifier) {
    return task_status_[identifier] == true;
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
    if (running_) {
      shutdown();
    }
    max_worker_threads_ = max;
    if (!running_)
      start();
  }

  ThreadPool<T> operator=(const ThreadPool<T> &other) = delete;
  ThreadPool(const ThreadPool<T> &other) = delete;

  ThreadPool<T> &operator=(ThreadPool<T> &&other) {
    std::lock_guard<std::recursive_mutex> lock(manager_mutex_);
    if (other.running_) {
      other.shutdown();
    }
    if (running_) {
      shutdown();
    }
    max_worker_threads_ = std::move(other.max_worker_threads_);
    daemon_threads_ = std::move(other.daemon_threads_);
    current_workers_ = 0;

    thread_queue_ = std::move(other.thread_queue_);
    worker_queue_ = std::move(other.worker_queue_);

    if (!running_) {
      start();
    }
    return *this;
  }

 protected:

  /**
   * Drain will notify tasks to stop following notification
   */
  void drain() {
    while (current_workers_ > 0) {
      tasks_available_.notify_one();
    }
  }
// determines if threads are detached
  bool daemon_threads_;
  // max worker threads
  int max_worker_threads_;
  // current worker tasks.
  std::atomic<int> current_workers_;
  // thread queue
  std::vector<std::thread> thread_queue_;
  // manager thread
  std::thread manager_thread_;
  // atomic running boolean
  std::atomic<bool> running_;
  // worker queue of worker objects
  moodycamel::ConcurrentQueue<Worker<T>> worker_queue_;
  std::priority_queue<Worker<T>, std::vector<Worker<T>>, WorkerComparator<T>> worker_priority_queue_;
  // notification for available work
  std::condition_variable tasks_available_;
  // map to identify if a task should be
  std::map<std::string, bool> task_status_;
  // manager mutex
  std::recursive_mutex manager_mutex_;
  // work queue mutex
  std::mutex worker_queue_mutex_;

  /**
   * Call for the manager to start worker threads
   */
  void startWorkers();

  /**
   * Runs worker tasks
   */
  void run_tasks();
};

template<typename T>
bool ThreadPool<T>::execute(Worker<T> &&task, std::future<T> &future) {

  {
    std::unique_lock<std::mutex> lock(worker_queue_mutex_);
    task_status_[task.getIdentifier()] = true;
  }
  future = std::move(task.getPromise()->get_future());
  bool enqueued = worker_queue_.enqueue(std::move(task));
  if (running_) {
    tasks_available_.notify_one();
  }
  return enqueued;
}

template<typename T>
void ThreadPool<T>::startWorkers() {
  for (int i = 0; i < max_worker_threads_; i++) {
    thread_queue_.push_back(std::move(std::thread(&ThreadPool::run_tasks, this)));
    current_workers_++;
  }

  if (daemon_threads_) {
    for (auto &thread : thread_queue_) {
      thread.detach();
    }
  }
  for (auto &thread : thread_queue_) {
    if (thread.joinable())
      thread.join();
  }
}
template<typename T>
void ThreadPool<T>::run_tasks() {
  auto waitperiod = std::chrono::milliseconds(1) * 100;
  uint64_t wait_decay_ = 0;
  while (running_.load()) {

    // if we exceed 500ms of wait due to not being able to run any tasks and there are tasks available, meaning
    // they are eligible to run per the fact that the thread pool isn't shut down and the tasks are in a runnable state
    // BUT they've been continually timesliced, we will lower the wait decay to 100ms and continue incrementing from
    // there. This ensures we don't have arbitrarily long sleep cycles. 
    if (wait_decay_ > 500000000L) {
      wait_decay_ = 100000000L;
    }
    // if we are spinning, perform a wait. If something changes in the worker such that the timeslice has changed, we will pick that information up. Note that it's possible
    // we could starve for processing time if all workers are waiting. In the event that the number of workers far exceeds the number of threads, threads will spin and potentially
    // wait until they arrive at a task that can be run. In this case we reset the wait_decay and attempt to pick up a new task. This means that threads that recently ran should
    // be more likely to run. This is intentional.

    if (wait_decay_ > 2000) {
      std::this_thread::sleep_for(std::chrono::nanoseconds(wait_decay_));
    }
    Worker<T> task;
    if (!worker_queue_.try_dequeue(task)) {
      std::unique_lock<std::mutex> lock(worker_queue_mutex_);
      if (worker_priority_queue_.size() > 0) {
        // this is safe as we are going to immediately pop the queue
        while (!worker_priority_queue_.empty()) {
          task = std::move(const_cast<Worker<T>&>(worker_priority_queue_.top()));
          worker_priority_queue_.pop();
          worker_queue_.enqueue(std::move(task));
          continue;
        }

      }
      tasks_available_.wait_for(lock, waitperiod);
      continue;
    } else {
      std::unique_lock<std::mutex> lock(worker_queue_mutex_);
      if (!task_status_[task.getIdentifier()]) {
        continue;
      }
    }

    bool wait_to_run = false;
    if (task.getTimeSlice() > 1) {
      double wt = (double) task.getWaitTime();
      auto now = std::chrono::system_clock::now().time_since_epoch();
      auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
      // if our differential is < 10% of the wait time we will not put the task into a wait state
      // since requeuing will break the time slice contract.
      if (task.getTimeSlice() > ms && (task.getTimeSlice() - ms) > (wt * .10)) {
        wait_to_run = true;
      }
    }
    // if we have to wait we re-queue the worker.
    if (wait_to_run) {
      {
        std::unique_lock<std::mutex> lock(worker_queue_mutex_);
        if (!task_status_[task.getIdentifier()]) {
          continue;
        }
        // put it on the priority queue
        worker_priority_queue_.push(std::move(task));
      }
      //worker_queue_.enqueue(std::move(task));

      wait_decay_ += 25;
      continue;
    }

    const bool task_renew = task.run();
    wait_decay_ = 0;
    if (task_renew) {

      {
        // even if we have more work to do we will not
        std::unique_lock<std::mutex> lock(worker_queue_mutex_);
        if (!task_status_[task.getIdentifier()]) {
          continue;
        }
      }
      worker_queue_.enqueue(std::move(task));
    }
  }
  current_workers_--;

}
template<typename T>
void ThreadPool<T>::start() {
  std::lock_guard<std::recursive_mutex> lock(manager_mutex_);
  if (!running_) {
    running_ = true;
    manager_thread_ = std::move(std::thread(&ThreadPool::startWorkers, this));
    if (worker_queue_.size_approx() > 0) {
      tasks_available_.notify_all();
    }
  }
}

template<typename T>
void ThreadPool<T>::stopTasks(const std::string &identifier) {
  std::unique_lock<std::mutex> lock(worker_queue_mutex_);
  task_status_[identifier] = false;
}

template<typename T>
void ThreadPool<T>::shutdown() {
  if (running_.load()) {
    std::lock_guard<std::recursive_mutex> lock(manager_mutex_);
    running_.store(false);

    drain();
    task_status_.clear();
    if (manager_thread_.joinable())
      manager_thread_.join();
    {
      std::unique_lock<std::mutex> lock(worker_queue_mutex_);
      thread_queue_.clear();
      current_workers_ = 0;
      while (worker_queue_.size_approx() > 0) {
        Worker<T> task;
        worker_queue_.try_dequeue(task);
      }
    }
  }
}

} /* namespace utils */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

#endif
