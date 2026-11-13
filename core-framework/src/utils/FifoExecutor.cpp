// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#include "utils/FifoExecutor.h"

namespace org::apache::nifi::minifi::utils::detail {
WorkerThread::WorkerThread()
    : thread_{&WorkerThread::run, this} {}

WorkerThread::~WorkerThread() {
  task_queue_.stop();
  thread_.join();
}

void WorkerThread::run() noexcept {
  while (task_queue_.isRunning()) {
    task_queue_.consumeWait([](std::packaged_task<void()>& f) { f(); });
  }
}
}  // namespace org::apache::nifi::minifi::utils::detail
