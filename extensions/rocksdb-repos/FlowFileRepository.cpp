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
#include "rocksdb/slice.h"
#include "utils/gsl.h"
#include "core/Resource.h"
#include "utils/OptionalUtils.h"
#include "core/TypedValues.h"
#include "FlowFileRecord.h"

using namespace std::literals::chrono_literals;

namespace org::apache::nifi::minifi::core::repository {

void FlowFileRepository::flush() {
  auto opendb = db_->open();
  if (!opendb) {
    return;
  }
  auto batch = opendb->createWriteBatch();

  std::list<ExpiredFlowFileInfo> flow_files;

  while (keys_to_delete_.size_approx() > 0) {
    ExpiredFlowFileInfo info;
    if (keys_to_delete_.try_dequeue(info)) {
      flow_files.push_back(std::move(info));
    }
  }

  deserializeFlowFilesWithNoContentClaim(opendb.value(), flow_files);

  for (auto& ff : flow_files) {
    batch.Delete(ff.key);
    logger_->log_debug("Issuing batch delete, including {}, Content path {}", ff.key, ff.content ? ff.content->getContentFullPath() : "null");
  }

  auto operation = [&batch, &opendb]() { return opendb->Write(rocksdb::WriteOptions(), &batch); };

  if (!ExecuteWithRetry(operation)) {
    for (auto&& ff : flow_files) {
      keys_to_delete_.enqueue(std::move(ff));
    }
    return;  // Stop here - don't delete from content repo while we have records in FF repo
  }

  if (content_repo_) {
    for (auto& ff : flow_files) {
      if (ff.content) {
        ff.content->decreaseFlowFileRecordOwnedCount();
      }
    }
  }
}

void FlowFileRepository::deserializeFlowFilesWithNoContentClaim(minifi::internal::OpenRocksDb& opendb, std::list<ExpiredFlowFileInfo>& flow_files) {
  std::vector<rocksdb::Slice> keys;
  std::vector<std::list<ExpiredFlowFileInfo>::iterator> key_positions;
  for (auto it = flow_files.begin(); it != flow_files.end(); ++it) {
    if (!it->content) {
      keys.push_back(it->key);
      key_positions.push_back(it);
    }
  }
  if (keys.empty()) {
    return;
  }
  std::vector<std::string> values;
  auto multistatus = opendb.MultiGet(rocksdb::ReadOptions{}, keys, &values);
  gsl_Expects(keys.size() == values.size() && values.size() == multistatus.size());

  for (size_t i = 0; i < keys.size(); ++i) {
    if (!multistatus[i].ok()) {
      logger_->log_error("Failed to read key from rocksdb: {}! DB is most probably in an inconsistent state!", keys[i].data());
      flow_files.erase(key_positions.at(i));
      continue;
    }

    utils::Identifier container_id;
    auto flow_file = FlowFileRecord::DeSerialize(gsl::make_span(values[i]).as_span<const std::byte>(), content_repo_, container_id);
    if (flow_file) {
      gsl_Expects(flow_file->getUUIDStr() == key_positions.at(i)->key);
      key_positions.at(i)->content = flow_file->getResourceClaim();
    } else {
      logger_->log_error("Could not deserialize flow file {}", key_positions.at(i)->key);
    }
  }
}

void FlowFileRepository::run() {
  while (isRunning()) {
    std::this_thread::sleep_for(purge_period_);
    flush();
  }
  flush();
}

bool FlowFileRepository::contentSizeIsAmpleForFlowFile(const FlowFile& flow_file_record, const std::shared_ptr<ResourceClaim>& resource_claim) const {
  const auto stream_size = resource_claim ? content_repo_->size(*resource_claim) : 0;
  const auto required_size = flow_file_record.getOffset() + flow_file_record.getSize();
  return stream_size >= required_size;
}

Connectable* FlowFileRepository::getContainer(const std::string& container_id) {
  auto container = containers_.find(container_id);
  if (container != containers_.end())
    return container->second;
  // for backward compatibility
  container = connection_map_.find(container_id);
  if (container != connection_map_.end())
    return container->second;
  return nullptr;
}
void FlowFileRepository::initialize_repository() {
  auto opendb = db_->open();
  if (!opendb) {
    logger_->log_trace("Couldn't open database to load existing flow files");
    return;
  }
  logger_->log_info("Reading existing flow files from database");

  const auto it = opendb->NewIterator(rocksdb::ReadOptions());
  for (it->SeekToFirst(); it->Valid(); it->Next()) {
    utils::Identifier container_id;
    auto eventRead = FlowFileRecord::DeSerialize(gsl::make_span(it->value()).as_span<const std::byte>(), content_repo_, container_id);
    const std::string key = it->key().ToString();
    if (!eventRead) {
      // failed to deserialize FlowFile, cannot clear claim
      keys_to_delete_.enqueue({.key = key});
      continue;
    }
    auto claim = eventRead->getResourceClaim();
    if (claim) {
      claim->increaseFlowFileRecordOwnedCount();
    }
    const auto container = getContainer(container_id.to_string());
    if (!container) {
      logger_->log_warn("Could not find connection for {}, path {}", container_id.to_string(), eventRead->getContentFullPath());
      keys_to_delete_.enqueue({.key = key, .content = eventRead->getResourceClaim()});
      continue;
    }
    if (check_flowfile_content_size_ && !contentSizeIsAmpleForFlowFile(*eventRead, claim)) {
      logger_->log_warn("Content is missing or too small for flowfile {}", eventRead->getContentFullPath());
      keys_to_delete_.enqueue({.key = key, .content = eventRead->getResourceClaim()});
      continue;
    }

    logger_->log_debug("Found connection for {}, path {}", container_id.to_string(), eventRead->getContentFullPath());
    eventRead->setStoredToRepository(true);
    // we found the connection for the persistent flowFile
    // even if a processor immediately marks it for deletion, flush only happens after prune_stored_flowfiles
    container->restore(eventRead);
  }
  flush();
  content_repo_->clearOrphans();
}

void FlowFileRepository::loadComponent(const std::shared_ptr<core::ContentRepository> &content_repo) {
  content_repo_ = content_repo;
  swap_loader_ = std::make_unique<FlowFileLoader>(gsl::make_not_null(db_.get()), content_repo_);

  initialize_repository();
}

namespace {
bool getRepositoryCheckHealth(const Configure& configure) {
  std::string check_health_str;
  configure.get(Configure::nifi_flow_file_repository_check_health, check_health_str);
  return utils::string::toBool(check_health_str).value_or(true);
}
}  // namespace

bool FlowFileRepository::initialize(const std::shared_ptr<Configure> &configure) {
  config_ = configure;
  std::string value;

  if (configure->get(Configure::nifi_flowfile_repository_directory_default, value) && !value.empty()) {
    directory_ = value;
  }
  check_flowfile_content_size_ = getRepositoryCheckHealth(*configure);
  logger_->log_debug("NiFi FlowFile Repository Directory {}", directory_);

  setCompactionPeriod(configure);

  const auto encrypted_env = createEncryptingEnv(utils::crypto::EncryptionManager{configure->getHome()}, DbEncryptionOptions{directory_, ENCRYPTION_KEY_NAME});
  logger_->log_info("Using {} FlowFileRepository", encrypted_env ? "encrypted" : "plaintext");

  auto db_options = [encrypted_env] (minifi::internal::Writable<rocksdb::DBOptions>& options) {
    minifi::internal::setCommonRocksDbOptions(options);
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
  db_ = minifi::internal::RocksDatabase::create(db_options, cf_options, directory_,
    minifi::internal::getRocksDbOptionsToOverride(configure, Configure::nifi_flowfile_repository_rocksdb_options));
  if (db_->open()) {
    logger_->log_debug("NiFi FlowFile Repository database open {} success", directory_);
    return true;
  } else {
    logger_->log_error("NiFi FlowFile Repository database open {} fail", directory_);
    return false;
  }
}

void FlowFileRepository::setCompactionPeriod(const std::shared_ptr<Configure> &configure) {
  compaction_period_ = DEFAULT_COMPACTION_PERIOD;
  if (auto compaction_period_str = configure->get(Configure::nifi_flowfile_repository_rocksdb_compaction_period)) {
    if (auto compaction_period = TimePeriodValue::fromString(compaction_period_str.value())) {
      compaction_period_ = compaction_period->getMilliseconds();
      if (compaction_period_.count() == 0) {
        logger_->log_warn("Setting '{}' to 0 disables forced compaction", Configure::nifi_flowfile_repository_rocksdb_compaction_period);
      }
    } else {
      logger_->log_error("Malformed property '{}', expected time period, using default", Configure::nifi_flowfile_repository_rocksdb_compaction_period);
    }
  } else {
    logger_->log_debug("Using default compaction period of {}", compaction_period_);
  }
}

bool FlowFileRepository::Delete(const std::string& key) {
  keys_to_delete_.enqueue({.key = key});
  return true;
}

void FlowFileRepository::runCompaction() {
  do {
    if (auto opendb = db_->open()) {
      auto status = opendb->RunCompaction();
      logger_->log_trace("Compaction triggered: {}", status.ToString());
    } else {
      logger_->log_error("Failed to open database for compaction");
    }
  } while (!utils::StoppableThread::waitForStopRequest(compaction_period_));
}

bool FlowFileRepository::start() {
  const bool ret = ThreadedRepositoryImpl::start();
  if (swap_loader_) {
    swap_loader_->start();
  }
  if (compaction_period_.count() != 0) {
    compaction_thread_ = std::make_unique<utils::StoppableThread>([this] () {
      runCompaction();
    });
  }
  return ret;
}

bool FlowFileRepository::stop() {
  compaction_thread_.reset();
  if (swap_loader_) {
    swap_loader_->stop();
  }
  return ThreadedRepositoryImpl::stop();
}

void FlowFileRepository::store(std::vector<std::shared_ptr<core::FlowFile>> flow_files) {
  gsl_Expects(ranges::all_of(flow_files, &FlowFile::isStored));
  // pass, flowfiles are already persisted in the repository
}

std::future<std::vector<std::shared_ptr<core::FlowFile>>> FlowFileRepository::load(std::vector<SwappedFlowFile> flow_files) {
  return swap_loader_->load(std::move(flow_files));
}

bool FlowFileRepository::Delete(const std::shared_ptr<core::CoreComponent>& item) {
  if (auto ff = std::dynamic_pointer_cast<core::FlowFile>(item)) {
    keys_to_delete_.enqueue({.key = item->getUUIDStr(), .content = ff->getResourceClaim()});
  } else {
    keys_to_delete_.enqueue({.key = item->getUUIDStr()});
  }
  return true;
}

REGISTER_RESOURCE_AS(FlowFileRepository, InternalResource, ("FlowFileRepository", "flowfilerepository"));

}  // namespace org::apache::nifi::minifi::core::repository
