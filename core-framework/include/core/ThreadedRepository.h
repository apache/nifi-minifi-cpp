// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#pragma once

#include <memory>
#include <thread>

#include "core/Repository.h"
#include "minifi-cpp/core/TraceableResource.h"
#include "minifi-cpp/core/ThreadedRepository.h"

namespace org::apache::nifi::minifi::core {

class ThreadedRepositoryImpl : public core::RepositoryImpl, public virtual core::ThreadedRepository {
 public:
  using RepositoryImpl::RepositoryImpl;

  ~ThreadedRepositoryImpl() override {
    if (running_state_.load() != RunningState::Stopped) {
      logger_->log_error("Thread of {} should have been stopped in subclass before ThreadedRepository's destruction", name_);
    }
  }

  bool initialize(const std::shared_ptr<Configure>& /*configure*/) override {
    return true;
  }

  // Starts repository monitor thread
  bool start() override {
    // if Stopped, turn to Starting, otherwise return
    RunningState expected{RunningState::Stopped};
    if (!running_state_.compare_exchange_strong(expected, RunningState::Starting)) {
      return false;
    }
    if (purge_period_ <= std::chrono::milliseconds(0)) {
      running_state_.store(RunningState::Running);
      return true;
    }

    // must set Running state before calling run(), as run() might check state
    running_state_.store(RunningState::Running);
    getThread() = std::thread(&ThreadedRepositoryImpl::run, this);

    logger_->log_debug("{} ThreadedRepository monitor thread start", name_);
    return true;
  }

  // Stops repository monitor thread
  bool stop() override {
    // if RUNNING, turn to STOPPING, otherwise return
    RunningState expected{RunningState::Running};
    if (!running_state_.compare_exchange_strong(expected, RunningState::Stopping)) {
      return false;
    }
    if (getThread().joinable()) {
      getThread().join();
    }
    running_state_.store(RunningState::Stopped);
    logger_->log_debug("{} ThreadedRepository monitor thread stop", name_);
    return true;
  }

  bool isRunning() const override {
    return running_state_.load() == RunningState::Running;
  }

  BackTrace getTraces() override {
    // If the thread is joinable then it is finished, we cannot interrupt and collect its stack traces
    // It can occur with an empty run() implementation like the NoOpThreadedRepository where the stack trace is unavailable
    if (getThread().joinable()) {
      return {};
    }
    return TraceResolver::getResolver().getBackTrace(getName(), getThread().native_handle());
  }

 private:
  virtual void run() = 0;

  /**
   * READ BEFORE USING!
   * @returns repository monitor thread
   * Thread-owning overriding subclasses MUST also call stop() in their destructor
   * to prevent the thread still using their members after they are are destructed (it's too late in the destructor of this base class)
   */
  virtual std::thread& getThread() = 0;

  enum class RunningState : uint8_t {
    Starting,
    Running,
    Stopping,
    Stopped
  };

  std::atomic<RunningState> running_state_{RunningState::Stopped};
  std::shared_ptr<logging::Logger> logger_ {logging::LoggerFactory<ThreadedRepository>::getLogger()};
};

}  // namespace org::apache::nifi::minifi::core
