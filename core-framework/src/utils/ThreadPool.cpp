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

#include "utils/ThreadPool.h"
#include "core/logging/LoggerFactory.h"

using namespace std::literals::chrono_literals;

namespace org::apache::nifi::minifi::utils {

ThreadPool::ThreadPool(int max_worker_threads, core::controller::ControllerServiceLookup* controller_service_provider, std::string name)
    : thread_reduction_count_(0),
      max_worker_threads_(max_worker_threads),
      current_workers_(0),
      adjust_threads_(false),
      running_(false),
      controller_service_provider_(controller_service_provider),
      name_(std::move(name)),
      logger_(core::logging::LoggerFactory<ThreadPool>::getLogger()) {
}

void ThreadPool::run_tasks(const std::shared_ptr<WorkerThread>& thread) {
  thread->is_running_ = true;
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

    Worker task;
    if (worker_queue_.dequeueWait(task)) {
      {
        std::unique_lock<std::mutex> lock(worker_queue_mutex_);
        if (!task_status_[task.getIdentifier()]) {
          continue;
        } else if (!worker_queue_.isRunning()) {
          worker_queue_.enqueue(std::move(task));
          continue;
        }
        ++running_task_count_by_id_[task.getIdentifier()];
      }
      const bool taskRunResult = task.run();
      {
        std::unique_lock<std::mutex> lock(worker_queue_mutex_);
        auto& count = running_task_count_by_id_[task.getIdentifier()];
        if (count == 1) {
          running_task_count_by_id_.erase(task.getIdentifier());
        } else {
          --count;
        }
      }
      task_run_complete_.notify_all();
      if (taskRunResult) {
        if (task.getNextExecutionTime() <= std::chrono::steady_clock::now()) {
          // it can be rescheduled again as soon as there is a worker available
          worker_queue_.enqueue(std::move(task));
          continue;
        }
        // Task will be put to the delayed queue as next exec time is in the future
        std::unique_lock<std::mutex> lock(worker_queue_mutex_);
        bool need_to_notify =
            delayed_worker_queue_.empty() ||
                task.getNextExecutionTime() < delayed_worker_queue_.top().getNextExecutionTime();

        delayed_worker_queue_.push(std::move(task));
        if (need_to_notify) {
          delayed_task_available_.notify_all();
        }
      }
    } else {
      // The threadpool is running, but the ConcurrentQueue is stopped -> shouldn't happen during normal conditions
      // Might happen during startup or shutdown for a very short time
      if (running_.load()) {
        std::this_thread::sleep_for(1ms);
      }
    }
  }
  current_workers_--;
}

void ThreadPool::manage_delayed_queue() {
  while (running_) {
    std::unique_lock<std::mutex> lock(worker_queue_mutex_);

    // Put the tasks ready to run in the worker queue
    while (!delayed_worker_queue_.empty() && delayed_worker_queue_.top().getNextExecutionTime() <= std::chrono::steady_clock::now()) {
      // I'm very sorry for this - committee must has been seriously drunk when the interface of prio queue was submitted.
      Worker task = std::move(const_cast<Worker&>(delayed_worker_queue_.top()));
      delayed_worker_queue_.pop();
      worker_queue_.enqueue(std::move(task));
    }
    if (delayed_worker_queue_.empty()) {
      delayed_task_available_.wait(lock);
    } else {
      auto wait_time = delayed_worker_queue_.top().getNextExecutionTime() - std::chrono::steady_clock::now();
      delayed_task_available_.wait_for(lock, std::max(wait_time, std::chrono::steady_clock::duration(1ms)));
    }
  }
}

void ThreadPool::execute(Worker &&task, std::future<utils::TaskRescheduleInfo> &future) {
  {
    std::unique_lock<std::mutex> lock(worker_queue_mutex_);
    task_status_[task.getIdentifier()] = true;
  }
  future = task.getPromise()->get_future();
  worker_queue_.enqueue(std::move(task));
}

void ThreadPool::manageWorkers() {
  {
    std::unique_lock<std::mutex> lock(worker_queue_mutex_);
    for (int i = 0; i < max_worker_threads_; i++) {
      std::stringstream thread_name;
      thread_name << name_ << " #" << i;
      auto worker_thread = std::make_shared<WorkerThread>(thread_name.str());
      worker_thread->thread_ = createThread([this, worker_thread] { run_tasks(worker_thread); });
      thread_queue_.push_back(worker_thread);
      current_workers_++;
    }
  }

  if (nullptr != thread_manager_) {
    while (running_) {
      auto wait_period = 500ms;
      {
        std::unique_lock<std::recursive_mutex> manager_lock(manager_mutex_, std::try_to_lock);
        if (!manager_lock.owns_lock()) {
          // Threadpool is being stopped/started or config is being changed, better wait a bit
          std::this_thread::sleep_for(10ms);
          continue;
        }
        if (thread_manager_->isAboveMax(current_workers_)) {
          auto max = thread_manager_->getMaxConcurrentTasks();
          auto differential = current_workers_ - max;
          thread_reduction_count_ += differential;
        } else if (thread_manager_->shouldReduce()) {
          if (current_workers_ > 1)
            thread_reduction_count_++;
          thread_manager_->reduce();
        } else if (thread_manager_->canIncrease() && max_worker_threads_ > current_workers_) {  // increase slowly
          std::unique_lock<std::mutex> worker_queue_lock(worker_queue_mutex_);
          auto worker_thread = std::make_shared<WorkerThread>();
          worker_thread->thread_ = createThread([this, worker_thread] { run_tasks(worker_thread); });
          thread_queue_.push_back(worker_thread);
          current_workers_++;
        }
        std::shared_ptr<WorkerThread> thread_ref;
        while (deceased_thread_queue_.tryDequeue(thread_ref)) {
          std::unique_lock<std::mutex> worker_queue_lock(worker_queue_mutex_);
          if (thread_ref->thread_.joinable())
            thread_ref->thread_.join();
          thread_queue_.erase(std::remove(thread_queue_.begin(), thread_queue_.end(), thread_ref), thread_queue_.end());
        }
      }
      std::this_thread::sleep_for(wait_period);
    }
  } else {
    for (auto &thread : thread_queue_) {
      if (thread->thread_.joinable())
        thread->thread_.join();
    }
  }
}

std::shared_ptr<controllers::ThreadManagementService> ThreadPool::createThreadManager() const {
  if (!controller_service_provider_) {
    return nullptr;
  }
  auto service = controller_service_provider_->getControllerService("ThreadPoolManager");
  if (!service) {
    logger_->log_info("Could not find a ThreadPoolManager service");
    return nullptr;
  }
  auto thread_manager_service = std::dynamic_pointer_cast<controllers::ThreadManagementService>(service);
  if (!thread_manager_service) {
    logger_->log_error("Found ThreadPoolManager, but it is not a ThreadManagementService");
    return nullptr;
  }
  return thread_manager_service;
}

void ThreadPool::start() {
  std::lock_guard<std::recursive_mutex> lock(manager_mutex_);
  if (!running_) {
    thread_manager_ = createThreadManager();

    running_ = true;
    worker_queue_.start();
    manager_thread_ = std::thread(&ThreadPool::manageWorkers, this);

    std::lock_guard<std::mutex> quee_lock(worker_queue_mutex_);
    delayed_scheduler_thread_ = std::thread(&ThreadPool::manage_delayed_queue, this);
  }
}

void ThreadPool::stopTasks(const TaskId &identifier) {
  std::unique_lock<std::mutex> lock(worker_queue_mutex_);
  task_status_[identifier] = false;

  // remove tasks belonging to identifier from worker_queue_
  worker_queue_.remove([&] (const Worker& worker) { return worker.getIdentifier() == identifier; });

  // also remove from delayed_worker_queue_
  decltype(delayed_worker_queue_) new_delayed_worker_queue;
  while (!delayed_worker_queue_.empty()) {
    Worker task = std::move(const_cast<Worker&>(delayed_worker_queue_.top()));
    delayed_worker_queue_.pop();
    if (task.getIdentifier() != identifier) {
      new_delayed_worker_queue.push(std::move(task));
    }
  }
  delayed_worker_queue_ = std::move(new_delayed_worker_queue);

  // if tasks are in progress, wait for their completion
  task_run_complete_.wait(lock, [&] () {
    auto iter = running_task_count_by_id_.find(identifier);
    return iter == running_task_count_by_id_.end() || iter->second == 0;
  });
}

void ThreadPool::resume() {
  if (!worker_queue_.isRunning()) {
    worker_queue_.start();
  }
}

void ThreadPool::pause() {
  if (worker_queue_.isRunning()) {
    worker_queue_.stop();
  }
}

void ThreadPool::shutdown() {
  if (running_.load()) {
    std::lock_guard<std::recursive_mutex> lock(manager_mutex_);
    running_.store(false);

    drain();

    task_status_.clear();
    if (manager_thread_.joinable()) {
      manager_thread_.join();
    }

    {
      // this lock ensures that the delayed_scheduler_thread_
      // is not between checking the running_ and before the cv_.wait*
      // as then, it would survive the notify_all call
      std::lock_guard<std::mutex> worker_lock(worker_queue_mutex_);
      delayed_task_available_.notify_all();
    }
    if (delayed_scheduler_thread_.joinable()) {
      delayed_scheduler_thread_.join();
    }

    for (const auto &thread : thread_queue_) {
      if (thread->thread_.joinable())
        thread->thread_.join();
    }

    thread_queue_.clear();
    current_workers_ = 0;
    while (!delayed_worker_queue_.empty()) {
      delayed_worker_queue_.pop();
    }

    worker_queue_.clear();
  }
}

}  // namespace org::apache::nifi::minifi::utils
