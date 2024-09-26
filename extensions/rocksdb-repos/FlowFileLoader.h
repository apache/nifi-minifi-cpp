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
#include <vector>
#include <memory>

#include "database/RocksDatabase.h"
#include "utils/gsl.h"
#include "core/ContentRepository.h"
#include "minifi-cpp/SwapManager.h"
#include "utils/ThreadPool.h"
#include "core/logging/Logger.h"

namespace org::apache::nifi::minifi {

class FlowFileLoader {
  using FlowFilePtr = std::shared_ptr<core::FlowFile>;
  using FlowFilePtrVec = std::vector<FlowFilePtr>;

  static constexpr size_t thread_count_ = 1;

 public:
  FlowFileLoader(gsl::not_null<minifi::internal::RocksDatabase*> db, std::shared_ptr<core::ContentRepository> content_repo);

  ~FlowFileLoader();

  void start();

  void stop();

  std::future<FlowFilePtrVec> load(std::vector<SwappedFlowFile> flow_files);

 private:
  utils::TaskRescheduleInfo loadImpl(const std::vector<SwappedFlowFile>& flow_files, const std::shared_ptr<std::promise<FlowFilePtrVec>>& output);

  utils::ThreadPool thread_pool_{thread_count_, nullptr, "FlowFileLoaderThreadPool"};

  gsl::not_null<minifi::internal::RocksDatabase*> db_;

  // TODO(adebreceni): shared_ptr is needed to call FlowFileRecord::Deserialize
  //    this ownership could be removed if that changes
  std::shared_ptr<core::ContentRepository> content_repo_;
  std::shared_ptr<core::logging::Logger> logger_;
};

}  // namespace org::apache::nifi::minifi
