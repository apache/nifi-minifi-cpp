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

#include <future>
#include <list>

#include "RocksDatabase.h"
#include "FlowFile.h"
#include "gsl.h"
#include "core/ContentRepository.h"
#include "SwapManager.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {

class FlowFileLoader {
  using FlowFilePtr = std::shared_ptr<core::FlowFile>;
  using FlowFilePtrVec = std::vector<FlowFilePtr>;

  static constexpr size_t thread_count_ = 1;

  struct Task {
    std::promise<FlowFilePtrVec> result;
    std::vector<SwappedFlowFile> flow_files;
  };

  struct Thread {
    std::future<void> terminated;
    std::thread impl;
  };

 public:
  FlowFileLoader();

  ~FlowFileLoader();

  void initialize(gsl::not_null<minifi::internal::RocksDatabase*> db, std::shared_ptr<core::ContentRepository> content_repo);

  void start();

  void stop();

  std::future<FlowFilePtrVec> load(std::vector<SwappedFlowFile> flow_files);

 private:
  void run(std::promise<void>&& terminated);

  std::mutex task_mtx_;
  std::list<Task> tasks_;
  std::condition_variable cond_var_;

  std::atomic<bool> running_{false};

  std::vector<Thread> threads_;

  minifi::internal::RocksDatabase* db_{nullptr};

  std::shared_ptr<core::ContentRepository> content_repo_;
  std::shared_ptr<logging::Logger> logger_;
};

}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
