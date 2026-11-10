// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#include "utils/StoppableThread.h"
#include "minifi-cpp/utils/gsl.h"

namespace org::apache::nifi::minifi::utils {

static thread_local StoppableThread* current_thread;

StoppableThread::StoppableThread(std::function<void()> fn) {
  thread_ = std::thread{[fn = std::move(fn), this] {
    current_thread = this;
    fn();
  }};
}

bool StoppableThread::waitForStopRequest(std::chrono::milliseconds time) {
  gsl_Expects(current_thread);
  std::unique_lock lock(current_thread->mtx_);
  // wait_for returns false if the predicate is still false, i.e. the thread is running
  return current_thread->cv_.wait_for(lock, time, [&] {return !current_thread->running_;});
}

}  // namespace org::apache::nifi::minifi::utils
