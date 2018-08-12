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
#include <sstream>
#include <iostream>
#include <atomic>
#include <mutex>
#include <map>
#include <vector>
#include <queue>
#include <future>
#include <thread>
#include <functional>

#include "BackTrace.h"
#include "core/expect.h"
#include "controllers/ThreadManagementService.h"
#include "concurrentqueue.h"
#include "core/controller/ControllerService.h"
#include "core/controller/ControllerServiceProvider.h"
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
      : identifier_(identifier),
        time_slice_(0),
        task(task),
        run_determinant_(std::move(run_determinant)) {
    promise = std::make_shared<std::promise<T>>();
  }

  explicit Worker(std::function<T()> &task, const std::string &identifier)
      : identifier_(identifier),
        time_slice_(0),
        task(task),
        run_determinant_(nullptr) {
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
      : identifier_(std::move(other.identifier_)),
        time_slice_(std::move(other.time_slice_)),
        task(std::move(other.task)),
        run_determinant_(std::move(other.run_determinant_)),
        promise(other.promise) {
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

class WorkerThread {
 public:
  explicit WorkerThread(std::thread thread, const std::string &name = "NamelessWorker")
      : is_running_(false),
        thread_(std::move(thread)),
        name_(name) {

  }
  WorkerThread(const std::string &name = "NamelessWorker")
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

  ThreadPool(int max_worker_threads = 2, bool daemon_threads = false, const std::shared_ptr<core::controller::ControllerServiceProvider> &controller_service_provider = nullptr,
             const std::string &name = "NamelessPool")
      : daemon_threads_(daemon_threads),
        thread_reduction_count_(0),
        max_worker_threads_(max_worker_threads),
        adjust_threads_(false),
        running_(false),
        controller_service_provider_(controller_service_provider),
        name_(name) {
    current_workers_ = 0;
    task_count_ = 0;
    thread_manager_ = nullptr;
  }

  ThreadPool(const ThreadPool<T> &&other)
      : daemon_threads_(std::move(other.daemon_threads_)),
        thread_reduction_count_(0),
        max_worker_threads_(std::move(other.max_worker_threads_)),
        adjust_threads_(false),
        running_(false),
        controller_service_provider_(std::move(other.controller_service_provider_)),
        thread_manager_(std::move(other.thread_manager_)),
        name_(std::move(other.name_)) {
    current_workers_ = 0;
    task_count_ = 0;
  }

  ~ThreadPool() {
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
    thread_reduction_count_ = 0;

    thread_queue_ = std::move(other.thread_queue_);
    worker_queue_ = std::move(other.worker_queue_);

    controller_service_provider_ = std::move(other.controller_service_provider_);
    thread_manager_ = std::move(other.thread_manager_);

    adjust_threads_ = false;

    if (!running_) {
      start();
    }

    name_ = other.name_;
    return *this;
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
    while (current_workers_ > 0) {
      tasks_available_.notify_one();
    }
  }
// determines if threads are detached
  bool daemon_threads_;
  std::atomic<int> thread_reduction_count_;
// max worker threads
  int max_worker_threads_;
// current worker tasks.
  std::atomic<int> current_workers_;
  std::atomic<int> task_count_;
// thread queue
  std::vector<std::shared_ptr<WorkerThread>> thread_queue_;
// manager thread
  std::thread manager_thread_;
// conditional that's used to adjust the threads
  std::atomic<bool> adjust_threads_;
// atomic running boolean
  std::atomic<bool> running_;
// controller service provider
  std::shared_ptr<core::controller::ControllerServiceProvider> controller_service_provider_;
// integrated power manager
  std::shared_ptr<controllers::ThreadManagementService> thread_manager_;
  // thread queue for the recently deceased threads.
  moodycamel::ConcurrentQueue<std::shared_ptr<WorkerThread>> deceased_thread_queue_;
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
  // thread pool name
  std::string name_;

  /**
   * Call for the manager to start worker threads
   */
  void manageWorkers();

  /**
   * Function to adjust the workers up and down.
   */
  void adjustWorkers(int count);

  /**
   * Runs worker tasks
   */
  void run_tasks(std::shared_ptr<WorkerThread> thread);
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

  task_count_++;

  return enqueued;
}

template<typename T>
void ThreadPool<T>::manageWorkers() {
  for (int i = 0; i < max_worker_threads_; i++) {
    std::stringstream thread_name;
    thread_name << name_ << " #" << i;
    auto worker_thread = std::make_shared<WorkerThread>(thread_name.str());
    worker_thread->thread_ = createThread(std::bind(&ThreadPool::run_tasks, this, worker_thread));
    thread_queue_.push_back(worker_thread);
    current_workers_++;
  }

  if (daemon_threads_) {
    for (auto &thread : thread_queue_) {
      thread->thread_.detach();
    }
  }

// likely don't have a thread manager
  if (LIKELY(nullptr != thread_manager_)) {
    while (running_) {
      auto waitperiod = std::chrono::milliseconds(1) * 500;
      {
        if (thread_manager_->isAboveMax(current_workers_)) {
          auto max = thread_manager_->getMaxConcurrentTasks();
          auto differential = current_workers_ - max;
          thread_reduction_count_ += differential;
        } else if (thread_manager_->shouldReduce()) {
          if (current_workers_ > 1)
            thread_reduction_count_++;
          thread_manager_->reduce();
        } else if (thread_manager_->canIncrease() && max_worker_threads_ - current_workers_ > 0) {  // increase slowly
          std::unique_lock<std::mutex> lock(worker_queue_mutex_);
          auto worker_thread = std::make_shared<WorkerThread>();
          worker_thread->thread_ = createThread(std::bind(&ThreadPool::run_tasks, this, worker_thread));
          if (daemon_threads_) {
            worker_thread->thread_.detach();
          }
          thread_queue_.push_back(worker_thread);
          current_workers_++;
        }
      }
      {
        std::shared_ptr<WorkerThread> thread_ref;
        while (deceased_thread_queue_.try_dequeue(thread_ref)) {
          std::unique_lock<std::mutex> lock(worker_queue_mutex_);
          if (thread_ref->thread_.joinable())
            thread_ref->thread_.join();
          thread_queue_.erase(std::remove(thread_queue_.begin(), thread_queue_.end(), thread_ref), thread_queue_.end());
        }
      }
      std::this_thread::sleep_for(waitperiod);
    }
  } else {
    for (auto &thread : thread_queue_) {
      if (thread->thread_.joinable())
        thread->thread_.join();
    }
  }
}
template<typename T>
void ThreadPool<T>::run_tasks(std::shared_ptr<WorkerThread> thread) {
  auto waitperiod = std::chrono::milliseconds(1) * 100;
  thread->is_running_ = true;
  uint64_t wait_decay_ = 0;
  uint64_t yield_backoff = 10;  // start at 10 ms
  while (running_.load()) {
    if (UNLIKELY(thread_reduction_count_ > 0)) {
      if (--thread_reduction_count_ >= 0) {
        deceased_thread_queue_.enqueue(thread);
        thread->is_running_ = false;
        break;
      } else {
        thread_reduction_count_++;
      }
    }
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

    if (current_workers_ < max_worker_threads_) {
      // we are in a reduced state. due to thread management
      // let's institute a backoff up to 500ms
      if (yield_backoff < 500) {
        yield_backoff += 10;
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(yield_backoff));
    } else {
      yield_backoff = 10;
    }
    Worker<T> task;

    bool prioritized_task = false;

    if (!prioritized_task) {
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
        if ((double) task.getTimeSlice() > ms && ((double) (task.getTimeSlice() - ms)) > (wt * .10)) {
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

        wait_decay_ += 25;
        continue;
      }
    }
    const bool task_renew = task.run();
    wait_decay_ = 0;
    if (task_renew) {

      if (UNLIKELY(task_count_ > current_workers_)) {
        // even if we have more work to do we will not
        std::unique_lock<std::mutex> lock(worker_queue_mutex_);
        if (!task_status_[task.getIdentifier()]) {
          continue;
        }

        worker_priority_queue_.push(std::move(task));
      } else {
        worker_queue_.enqueue(std::move(task));
      }
    }
  }
  current_workers_--;
}
template<typename T>
void ThreadPool<T>::start() {
  if (nullptr != controller_service_provider_) {
    auto thread_man = controller_service_provider_->getControllerService("ThreadPoolManager");
    thread_manager_ = thread_man != nullptr ? std::dynamic_pointer_cast<controllers::ThreadManagementService>(thread_man) : nullptr;
  } else {
    thread_manager_ = nullptr;
  }
  std::lock_guard<std::recursive_mutex> lock(manager_mutex_);
  if (!running_) {
    running_ = true;
    manager_thread_ = std::move(std::thread(&ThreadPool::manageWorkers, this));
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
      for(const auto &thread : thread_queue_){
        if (thread->thread_.joinable())
        thread->thread_.join();
      }
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
