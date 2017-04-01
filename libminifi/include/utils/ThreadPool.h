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

#include <iostream>
#include <atomic>
#include <mutex>
#include <vector>
#include <queue>
#include <future>
#include <thread>
namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace utils {

/**
 * Worker task
 * purpose: Provides a wrapper for the functor
 * and returns a future based on the template argument.
 */
template<typename T>
class Worker {
 public:
  explicit Worker(std::function<T()> &task)
      : task(task) {
    promise = std::make_shared<std::promise<T>>();
  }

  /**
   * Move constructor for worker tasks
   */
  Worker(Worker &&other)
      : task(std::move(other.task)),
        promise(other.promise) {
  }

  /**
   * Runs the task and takes the output from the funtor
   * setting the result into the promise
   */
  void run() {
    T result = task();
    promise->set_value(result);
  }

  Worker<T>(const Worker<T>&) = delete;
  Worker<T>& operator =(const Worker<T>&) = delete;

  Worker<T>& operator =(Worker<T> &&);

  std::shared_ptr<std::promise<T>> getPromise();

 private:
  std::function<T()> task;
  std::shared_ptr<std::promise<T>> promise;
};

template<typename T>
Worker<T>& Worker<T>::operator =(Worker<T> && other) {
  task = std::move(other.task);
  promise = other.promise;
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

  ThreadPool(int max_worker_threads = 8, bool daemon_threads = false)
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
   * @return future with the impending result.
   */
  std::future<T> execute(Worker<T> &&task);
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
  std::queue<Worker<T>> worker_queue_;
// notification for available work
  std::condition_variable tasks_available_;
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
}
;

template<typename T>
std::future<T> ThreadPool<T>::execute(Worker<T> &&task) {

  std::unique_lock<std::mutex> lock(worker_queue_mutex_);
  bool wasEmpty = worker_queue_.empty();
  std::future<T> future = task.getPromise()->get_future();
  worker_queue_.push(std::move(task));
  if (wasEmpty) {
    tasks_available_.notify_one();
  }
  return future;
}

template<typename T>
void ThreadPool<T>::startWorkers() {
  for (int i = 0; i < max_worker_threads_; i++) {
    thread_queue_.push_back(std::thread(&ThreadPool::run_tasks, this));
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
  while (running_.load()) {
    std::unique_lock<std::mutex> lock(worker_queue_mutex_);
    if (worker_queue_.empty()) {

      tasks_available_.wait(lock);
    }

    if (!running_.load())
      break;

    if (worker_queue_.empty())
      continue;
    Worker<T> task = std::move(worker_queue_.front());
    worker_queue_.pop();
    task.run();
  }
  current_workers_--;

}
template<typename T>
void ThreadPool<T>::start() {
  std::lock_guard<std::recursive_mutex> lock(manager_mutex_);
  if (!running_) {
    running_ = true;
    manager_thread_ = std::thread(&ThreadPool::startWorkers, this);

  }
}

template<typename T>
void ThreadPool<T>::shutdown() {

  std::lock_guard<std::recursive_mutex> lock(manager_mutex_);
  if (running_.load()) {

    running_.store(false);

    drain();
    if (manager_thread_.joinable())
      manager_thread_.join();
    {
      std::unique_lock<std::mutex> lock(worker_queue_mutex_);
      thread_queue_.clear();
      current_workers_ = 0;
      while (!worker_queue_.empty())
        worker_queue_.pop();
    }
  }
}

} /* namespace utils */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

#endif
