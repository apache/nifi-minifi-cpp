// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#pragma once

#include <future>
#include <list>
#include <vector>
#include <memory>

#include "database/RocksDatabase.h"
#include "minifi-cpp/utils/gsl.h"
#include "minifi-cpp/core/ContentRepository.h"
#include "minifi-cpp/SwapManager.h"
#include "utils/ThreadPool.h"
#include "minifi-cpp/core/logging/Logger.h"

namespace org::apache::nifi::minifi {

class FlowFileLoader {
  using FlowFilePtr = std::shared_ptr<core::FlowFile>;
  using FlowFilePtrVec = std::vector<FlowFilePtr>;

  static constexpr size_t thread_count_ = 1;

 public:
  FlowFileLoader(gsl::not_null<minifi::internal::RocksDatabase*> db, std::shared_ptr<core::ContentRepository> content_repo, bool verify_checksums_in_rocksdb_reads);

  ~FlowFileLoader();

  void start();

  void stop();

  std::future<FlowFilePtrVec> load(std::vector<SwappedFlowFile> flow_files);

 private:
  utils::TaskRescheduleInfo loadImpl(const std::vector<SwappedFlowFile>& flow_files, const std::shared_ptr<std::promise<FlowFilePtrVec>>& output);

  utils::ThreadPool thread_pool_{thread_count_, "FlowFileLoaderThreadPool"};

  gsl::not_null<minifi::internal::RocksDatabase*> db_;

  // TODO(adebreceni): shared_ptr is needed to call FlowFileRecord::Deserialize
  //    this ownership could be removed if that changes
  std::shared_ptr<core::ContentRepository> content_repo_;
  std::shared_ptr<core::logging::Logger> logger_;
  bool verify_checksums_in_rocksdb_reads_ = false;
};

}  // namespace org::apache::nifi::minifi
