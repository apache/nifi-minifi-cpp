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
#include "logging/LoggerConfiguration.h"
#include "FlowFileRecord.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {

FlowFileLoader::FlowFileLoader()
  : logger_(logging::LoggerFactory<FlowFileLoader>::getLogger()) {}

FlowFileLoader::~FlowFileLoader() {
  stop();
}

void FlowFileLoader::initialize(gsl::not_null<minifi::internal::RocksDatabase *> db, std::shared_ptr<core::ContentRepository> content_repo) {
  db_ = db;
  content_repo_ = std::move(content_repo);
}

std::future<FlowFileLoader::FlowFilePtrVec> FlowFileLoader::load(std::vector<SwappedFlowFile> flow_files) {
  std::future<FlowFilePtrVec> future;
  std::promise<FlowFilePtrVec> promise;
  future = promise.get_future();
  Task task{std::move(promise), std::move(flow_files)};
  {
    std::unique_lock<std::mutex> guard(task_mtx_);
    tasks_.push_back(std::move(task));
  }
  cond_var_.notify_one();
  return future;
}

void FlowFileLoader::start() {
  if (running_) {
    return;
  }
  running_ = true;
  for (size_t i = 0; i < thread_count_; ++i) {
    std::promise<void> terminated_p;
    std::future<void> terminated_f = terminated_p.get_future();
    threads_.push_back(Thread{std::move(terminated_f), std::thread{&FlowFileLoader::run, this, std::move(terminated_p)}});
    logger_->log_debug("Started swapping thread %zu of %zu", i, static_cast<size_t>(thread_count_));
  }
}

void FlowFileLoader::stop() {
  if (!running_) {
    return;
  }
  running_ = false;
  for (size_t i = 0; i < threads_.size(); ++i) {
    while (true) {
      cond_var_.notify_all();
      auto status = threads_[i].terminated.wait_for(std::chrono::milliseconds{100});
      if (status == std::future_status::ready) {
        break;
      } else if (status != std::future_status::timeout) {
        throw std::logic_error("Unknown future status, maybe deferred?");
      }
    }
    if (threads_[i].impl.joinable()) {
      threads_[i].impl.join();
    }
    logger_->log_debug("Stopped swapping thread %zu of %zu", i, threads_.size());
  }
  threads_.clear();
}

void FlowFileLoader::run(std::promise<void>&& terminated) {
  while (running_) {
    utils::optional<Task> opt_task;
    {
      std::unique_lock<std::mutex> lock(task_mtx_);
      cond_var_.wait(lock, [&] {return !tasks_.empty() || !running_;});
      if (!running_) {
        break;
      }
      opt_task = std::move(tasks_.front());
      tasks_.pop_front();
    }
    Task task = std::move(opt_task.value());
    utils::optional<FlowFilePtrVec> result = [&] () -> utils::optional<FlowFilePtrVec> {
      auto opendb = db_->open();
      if (!opendb) {
        return utils::nullopt;
      }
      try {
        FlowFilePtrVec result;
        result.reserve(task.flow_files.size());
        rocksdb::ReadOptions read_options;
        std::vector<utils::SmallString<36>> serialized_keys;
        serialized_keys.reserve(task.flow_files.size());
        for (const auto& item : task.flow_files) {
          serialized_keys.push_back(item.id.to_string());
        }
        std::vector<rocksdb::Slice> keys;
        keys.reserve(task.flow_files.size());
        for (size_t idx = 0; idx < task.flow_files.size(); ++idx) {
          keys.emplace_back(serialized_keys[idx].data(), serialized_keys[idx].length());
        }
        std::vector<std::string> serialized_items;
        serialized_items.reserve(task.flow_files.size());
        std::vector<rocksdb::Status> statuses = opendb->MultiGet(read_options, keys, &serialized_items);
        for (size_t idx = 0; idx < statuses.size(); ++idx) {
          if (!statuses[idx].ok()) {
            // TODO(adebreceni): handle missing flow file gracefully
            logger_->log_error("Failed to fetch flow file \"%s\"", serialized_keys[idx]);
            return utils::nullopt;
          }
          utils::Identifier container_id;
          auto flow_file = FlowFileRecord::DeSerialize(
              reinterpret_cast<const uint8_t*>(serialized_items[idx].data()),
              serialized_items[idx].size(), content_repo_, container_id);
          if (!flow_file) {
            // TODO(adebreceni): handle malformed flow file gracefully
            logger_->log_error("Failed to deserialize flow file \"%s\"", serialized_keys[idx]);
          } else {
            flow_file->setStoredToRepository(true);
            flow_file->setPenaltyExpiration(task.flow_files[idx].to_be_processed_after);
            result.push_back(std::move(flow_file));
            logger_->log_debug("Deserialized flow file \"%s\"", serialized_keys[idx]);
          }
        }
        return result;
      } catch (const std::exception& err) {
        logger_->log_error("Error while swapping flow files in: %s", err.what());
        return utils::nullopt;
      }
    }();
    if (!result) {
      logger_->log_error("Couldn't swap-in requested flow files");
      // put back task and retry again
      {
        std::lock_guard<std::mutex> guard(task_mtx_);
        tasks_.push_back(std::move(task));
      }
    } else {
      logger_->log_debug("Swapped-in %zu flow files", result->size());
      // we can now resolve the promise
      task.result.set_value(std::move(result.value()));
    }
  }
  terminated.set_value();
}

}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
