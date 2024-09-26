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

#include "FlowFileLoader.h"

#include <span>
#include <memory>
#include <string>
#include <vector>
#include <utility>

#include "core/logging/LoggerFactory.h"
#include "utils/gsl.h"
#include "FlowFileRecord.h"

namespace org::apache::nifi::minifi {

FlowFileLoader::FlowFileLoader(gsl::not_null<minifi::internal::RocksDatabase*> db, std::shared_ptr<core::ContentRepository> content_repo)
  : db_(db),
    content_repo_(std::move(content_repo)),
    logger_(core::logging::LoggerFactory<FlowFileLoader>::getLogger()) {}

FlowFileLoader::~FlowFileLoader() {
  stop();
}

std::future<FlowFileLoader::FlowFilePtrVec> FlowFileLoader::load(std::vector<SwappedFlowFile> flow_files) {
  auto promise = std::make_shared<std::promise<FlowFilePtrVec>>();
  std::future<FlowFilePtrVec> future = promise->get_future();
  utils::Worker task{[this, flow_files = std::move(flow_files), promise = std::move(promise)] {
      return loadImpl(flow_files, promise);
    },
    ""};  // doesn't matter that tasks alias by name, as we never actually query their status or stop a single task

  // the dummy_future is for the return value of the Worker's lambda, rerunning this lambda
  // depends on run_determinant + result
  // we could create a custom run_determinant to instead determine if/when it should be rerun
  // based on the lambda's return value (e.g. it could return a nonstd::expected<FlowFilePtrVec, TaskRescheduleInfo>)
  // but then the std::future would also bear this type
  std::future<utils::TaskRescheduleInfo> dummy_future;
  thread_pool_.execute(std::move(task), dummy_future);
  return future;
}

void FlowFileLoader::start() {
  thread_pool_.start();
}

void FlowFileLoader::stop() {
  thread_pool_.shutdown();
}

utils::TaskRescheduleInfo FlowFileLoader::loadImpl(const std::vector<SwappedFlowFile>& flow_files, const std::shared_ptr<std::promise<FlowFilePtrVec>>& output) {
  auto opendb = db_->open();
  if (!opendb) {
    logger_->log_error("Couldn't open database to swap-in flow files");
    return utils::TaskRescheduleInfo::RetryIn(std::chrono::seconds{30});
  }
  try {
    FlowFilePtrVec result;
    result.reserve(flow_files.size());
    rocksdb::ReadOptions read_options;
    std::vector<utils::SmallString<36>> serialized_keys;
    serialized_keys.reserve(flow_files.size());
    for (const auto& item : flow_files) {
      serialized_keys.push_back(item.id.to_string());
    }
    std::vector<rocksdb::Slice> keys;
    keys.reserve(flow_files.size());
    for (size_t idx = 0; idx < flow_files.size(); ++idx) {
      keys.emplace_back(serialized_keys[idx].data(), serialized_keys[idx].length());
    }
    std::vector<std::string> serialized_items;
    serialized_items.reserve(flow_files.size());
    std::vector<rocksdb::Status> statuses = opendb->MultiGet(read_options, keys, &serialized_items);
    for (size_t idx = 0; idx < statuses.size(); ++idx) {
      if (!statuses[idx].ok()) {
        logger_->log_error("Failed to fetch flow file \"{}\"", serialized_keys[idx]);
        return utils::TaskRescheduleInfo::RetryIn(std::chrono::seconds{30});
      }
      utils::Identifier container_id;
      auto flow_file = FlowFileRecord::DeSerialize(
          gsl::make_span(serialized_items[idx]).as_span<const std::byte>(), content_repo_, container_id);
      if (!flow_file) {
        // corrupted flow file
        logger_->log_error("Failed to deserialize flow file \"{}\"", serialized_keys[idx]);
      } else {
        flow_file->setStoredToRepository(true);
        flow_file->setPenaltyExpiration(flow_files[idx].to_be_processed_after);
        result.push_back(std::move(flow_file));
        logger_->log_debug("Deserialized flow file \"{}\"", serialized_keys[idx]);
      }
    }
    output->set_value(result);
    return utils::TaskRescheduleInfo::Done();
  } catch (const std::exception& err) {
    logger_->log_error("Error while swapping flow files in: {}", err.what());
    return utils::TaskRescheduleInfo::RetryIn(std::chrono::seconds{60});
  }
}

}  // namespace org::apache::nifi::minifi
