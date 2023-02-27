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
#include "FlowFileRepository.h"

#include <chrono>
#include <list>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "rocksdb/options.h"
#include "rocksdb/write_batch.h"
#include "rocksdb/slice.h"
#include "FlowFileRecord.h"
#include "utils/gsl.h"
#include "core/Resource.h"
#include "utils/ThreadUtils.h"

using namespace std::literals::chrono_literals;

namespace org::apache::nifi::minifi::core::repository {

void FlowFileRepository::flush() {
  auto opendb = db_->open();
  if (!opendb) {
    return;
  }
  auto batch = opendb->createWriteBatch();
  rocksdb::ReadOptions options;

  std::vector<std::shared_ptr<FlowFile>> purgeList;

  std::vector<rocksdb::Slice> keys;
  std::list<std::string> keystrings;
  std::vector<std::string> values;

  while (keys_to_delete.size_approx() > 0) {
    std::string key;
    if (keys_to_delete.try_dequeue(key)) {
      keystrings.push_back(std::move(key));  // rocksdb::Slice doesn't copy the string, only grabs ptrs. Hacky, but have to ensure the required lifetime of the strings.
      keys.push_back(keystrings.back());
    }
  }
  auto multistatus = opendb->MultiGet(options, keys, &values);

  for (size_t i = 0; i < keys.size() && i < values.size() && i < multistatus.size(); ++i) {
    if (!multistatus[i].ok()) {
      logger_->log_error("Failed to read key from rocksdb: %s! DB is most probably in an inconsistent state!", keys[i].data());
      keystrings.remove(keys[i].data());
      continue;
    }

    utils::Identifier containerId;
    auto eventRead = FlowFileRecord::DeSerialize(gsl::make_span(values[i]).as_span<const std::byte>(), content_repo_, containerId);
    if (eventRead) {
      purgeList.push_back(eventRead);
    }
    logger_->log_debug("Issuing batch delete, including %s, Content path %s", eventRead->getUUIDStr(), eventRead->getContentFullPath());
    batch.Delete(keys[i]);
  }

  auto operation = [&batch, &opendb]() { return opendb->Write(rocksdb::WriteOptions(), &batch); };

  if (!ExecuteWithRetry(operation)) {
    for (const auto& key : keystrings) {
      keys_to_delete.enqueue(key);  // Push back the values that we could get but couldn't delete
    }
    return;  // Stop here - don't delete from content repo while we have records in FF repo
  }

  if (content_repo_) {
    for (const auto &ffr : purgeList) {
      auto claim = ffr->getResourceClaim();
      if (claim) claim->decreaseFlowFileRecordOwnedCount();
    }
  }
}

void FlowFileRepository::printStats() {
  auto opendb = db_->open();
  if (!opendb) {
    return;
  }
  std::string key_count;
  opendb->GetProperty("rocksdb.estimate-num-keys", &key_count);

  std::string table_readers;
  opendb->GetProperty("rocksdb.estimate-table-readers-mem", &table_readers);

  std::string all_memtables;
  opendb->GetProperty("rocksdb.cur-size-all-mem-tables", &all_memtables);

  logger_->log_info("Repository stats: key count: %s, table readers size: %s, all memory tables size: %s",
      key_count, table_readers, all_memtables);
}

void FlowFileRepository::run() {
  auto last = std::chrono::steady_clock::now();
  while (isRunning()) {
    std::this_thread::sleep_for(purge_period_);
    flush();
    auto now = std::chrono::steady_clock::now();
    if ((now-last) > std::chrono::seconds(30)) {
      printStats();
      last = now;
    }
  }
  flush();
}

bool FlowFileRepository::ExecuteWithRetry(const std::function<rocksdb::Status()>& operation) {
  constexpr int RETRY_COUNT = 3;
  std::chrono::milliseconds wait_time = 0ms;
  for (int i=0; i < RETRY_COUNT; ++i) {
    auto status = operation();
    if (status.ok()) {
      logger_->log_trace("Rocksdb operation executed successfully");
      return true;
    }
    logger_->log_error("Rocksdb operation failed: %s", status.ToString());
    wait_time += FLOWFILE_REPOSITORY_RETRY_INTERVAL_INCREMENTS;
    std::this_thread::sleep_for(wait_time);
  }
  return false;
}

void FlowFileRepository::initialize_repository() {
  auto opendb = db_->open();
  if (!opendb) {
    logger_->log_trace("Couldn't open database to load existing flow files");
    return;
  }
  logger_->log_info("Reading existing flow files from database");

  auto it = opendb->NewIterator(rocksdb::ReadOptions());
  for (it->SeekToFirst(); it->Valid(); it->Next()) {
    utils::Identifier containerId;
    auto eventRead = FlowFileRecord::DeSerialize(gsl::make_span(it->value()).as_span<const std::byte>(), content_repo_, containerId);
    std::string key = it->key().ToString();
    if (eventRead) {
      // on behalf of the just resurrected persisted instance
      auto claim = eventRead->getResourceClaim();
      if (claim) claim->increaseFlowFileRecordOwnedCount();
      bool found = false;
      auto search = containers_.find(containerId.to_string());
      found = (search != containers_.end());
      if (!found) {
        // for backward compatibility
        search = connection_map_.find(containerId.to_string());
        found = (search != connection_map_.end());
      }
      if (found) {
        logger_->log_debug("Found connection for %s, path %s ", containerId.to_string(), eventRead->getContentFullPath());
        eventRead->setStoredToRepository(true);
        // we found the connection for the persistent flowFile
        // even if a processor immediately marks it for deletion, flush only happens after prune_stored_flowfiles
        search->second->restore(eventRead);
      } else {
        logger_->log_warn("Could not find connection for %s, path %s ", containerId.to_string(), eventRead->getContentFullPath());
        keys_to_delete.enqueue(key);
      }
    } else {
      // failed to deserialize FlowFile, cannot clear claim
      keys_to_delete.enqueue(key);
    }
  }
  flush();
  content_repo_->clearOrphans();
}

void FlowFileRepository::loadComponent(const std::shared_ptr<core::ContentRepository> &content_repo) {
  content_repo_ = content_repo;
  repo_size_ = 0;
  swap_loader_ = std::make_unique<FlowFileLoader>(gsl::make_not_null(db_.get()), content_repo_);

  initialize_repository();
}

bool FlowFileRepository::initialize(const std::shared_ptr<Configure> &configure) {
  config_ = configure;
  std::string value;

  if (configure->get(Configure::nifi_flowfile_repository_directory_default, value) && !value.empty()) {
    directory_ = value;
  }
  logger_->log_debug("NiFi FlowFile Repository Directory %s", directory_);

  compaction_period_ = DEFAULT_COMPACTION_PERIOD;
  if (auto compaction_period_str = configure->get(Configure::nifi_flowfile_repository_compaction_period)) {
    if (auto compaction_period = TimePeriodValue::fromString(compaction_period_str.value())) {
      compaction_period_ = compaction_period->getMilliseconds();
      if (compaction_period_.count() == 0) {
        logger_->log_warn("Setting '%s' to 0 disables forced compaction", Configure::nifi_dbcontent_repository_compaction_period);
      }
    } else {
      logger_->log_error("Malformed property '%s', expected time period, using default", Configure::nifi_flowfile_repository_compaction_period);
    }
  } else {
    logger_->log_info("Using default compaction period");
  }

  const auto encrypted_env = createEncryptingEnv(utils::crypto::EncryptionManager{configure->getHome()}, DbEncryptionOptions{directory_, ENCRYPTION_KEY_NAME});
  logger_->log_info("Using %s FlowFileRepository", encrypted_env ? "encrypted" : "plaintext");

  auto db_options = [encrypted_env] (minifi::internal::Writable<rocksdb::DBOptions>& options) {
    options.set(&rocksdb::DBOptions::create_if_missing, true);
    options.set(&rocksdb::DBOptions::use_direct_io_for_flush_and_compaction, true);
    options.set(&rocksdb::DBOptions::use_direct_reads, true);
    if (encrypted_env) {
      options.set(&rocksdb::DBOptions::env, encrypted_env.get(), EncryptionEq{});
    } else {
      options.set(&rocksdb::DBOptions::env, rocksdb::Env::Default());
    }
  };

  // Write buffers are used as db operation logs. When they get filled the events are merged and serialized.
  // The default size is 64MB.
  // In our case it's usually too much, causing sawtooth in memory consumption. (Consumes more than the whole MiniFi)
  // To avoid DB write issues during heavy load it's recommended to have high number of buffer.
  // Rocksdb's stall feature can also trigger in case the number of buffers is >= 3.
  // The more buffers we have the more memory rocksdb can utilize without significant memory consumption under low load.
  auto cf_options = [&configure] (rocksdb::ColumnFamilyOptions& cf_opts) {
    cf_opts.OptimizeForPointLookup(4);
    cf_opts.write_buffer_size = 8ULL << 20U;
    cf_opts.max_write_buffer_number = 20;
    cf_opts.min_write_buffer_number_to_merge = 1;
    if (auto compression_type = minifi::internal::readConfiguredCompressionType(configure, Configure::nifi_flow_repository_rocksdb_compression)) {
      cf_opts.compression = *compression_type;
    }
  };
  db_ = minifi::internal::RocksDatabase::create(db_options, cf_options, directory_);
  if (db_->open()) {
    logger_->log_debug("NiFi FlowFile Repository database open %s success", directory_);
    return true;
  } else {
    logger_->log_error("NiFi FlowFile Repository database open %s fail", directory_);
    return false;
  }
}

bool FlowFileRepository::Put(const std::string& key, const uint8_t *buf, size_t bufLen) {
  // persistent to the DB
  auto opendb = db_->open();
  if (!opendb) {
    return false;
  }
  rocksdb::Slice value((const char *) buf, bufLen);
  auto operation = [&key, &value, &opendb]() { return opendb->Put(rocksdb::WriteOptions(), key, value); };
  return ExecuteWithRetry(operation);
}

bool FlowFileRepository::MultiPut(const std::vector<std::pair<std::string, std::unique_ptr<minifi::io::BufferStream>>>& data) {
  auto opendb = db_->open();
  if (!opendb) {
    return false;
  }
  auto batch = opendb->createWriteBatch();
  for (const auto &item : data) {
    const auto buf = item.second->getBuffer().as_span<const char>();
    rocksdb::Slice value(buf.data(), buf.size());
    if (!batch.Put(item.first, value).ok()) {
      logger_->log_error("Failed to add item to batch operation");
      return false;
    }
  }
  auto operation = [&batch, &opendb]() { return opendb->Write(rocksdb::WriteOptions(), &batch); };
  return ExecuteWithRetry(operation);
}

bool FlowFileRepository::Delete(const std::string& key) {
  keys_to_delete.enqueue(key);
  return true;
}

bool FlowFileRepository::Get(const std::string &key, std::string &value) {
  auto opendb = db_->open();
  if (!opendb) {
    return false;
  }
  return opendb->Get(rocksdb::ReadOptions(), key, &value).ok();
}

void FlowFileRepository::runCompaction(std::stop_token stop_token) {
  while (!stop_token.stop_requested()) {
    if (auto opendb = db_->open()) {
      auto status = opendb->RunCompaction();
      logger_->log_trace("Compaction triggered: %s", status.ToString());
    } else {
      logger_->log_error("Failed to open database for compaction");
    }
    utils::sleep_for(stop_token, compaction_period_);
  }
}

bool FlowFileRepository::start() {
  const bool ret = ThreadedRepository::start();
  if (swap_loader_) {
    swap_loader_->start();
  }
  if (compaction_period_.count() != 0) {
    compaction_thread_ = std::jthread([&] (std::stop_token stop_token) {
      runCompaction(std::move(stop_token));
    });
  }
  return ret;
}

bool FlowFileRepository::stop() {
  compaction_thread_.request_stop();
  if (compaction_thread_.joinable()) {
    compaction_thread_.join();
  }
  if (swap_loader_) {
    swap_loader_->stop();
  }
  return ThreadedRepository::stop();
}

void FlowFileRepository::store(std::vector<std::shared_ptr<core::FlowFile>> flow_files) {
  gsl_Expects(ranges::all_of(flow_files, &FlowFile::isStored));
  // pass, flowfiles are already persisted in the repository
}

std::future<std::vector<std::shared_ptr<core::FlowFile>>> FlowFileRepository::load(std::vector<SwappedFlowFile> flow_files) {
  return swap_loader_->load(std::move(flow_files));
}

REGISTER_RESOURCE_AS(FlowFileRepository, InternalResource, ("FlowFileRepository", "flowfilerepository"));

}  // namespace org::apache::nifi::minifi::core::repository
