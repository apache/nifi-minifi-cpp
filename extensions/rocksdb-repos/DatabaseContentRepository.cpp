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

#include "DatabaseContentRepository.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>
#include <cinttypes>

#include "encryption/RocksDbEncryptionProvider.h"
#include "RocksDbStream.h"
#include "utils/gsl.h"
#include "Exception.h"
#include "database/RocksDbUtils.h"
#include "database/StringAppender.h"
#include "core/Resource.h"
#include "core/TypedValues.h"

namespace org::apache::nifi::minifi::core::repository {

bool DatabaseContentRepository::initialize(const std::shared_ptr<minifi::Configure> &configuration) {
  std::string value;
  if (configuration->get(Configure::nifi_dbcontent_repository_directory_default, value) && !value.empty()) {
    directory_ = value;
  } else {
    directory_ = (configuration->getHome() / "dbcontentrepository").string();
  }
  auto purge_period_str = utils::string::trim(configuration->get(Configure::nifi_dbcontent_repository_purge_period).value_or("1 s"));
  if (purge_period_str == "0") {
    purge_period_ = std::chrono::seconds{0};
  } else if (auto purge_period_val = core::TimePeriodValue::fromString(purge_period_str)) {
    purge_period_ = purge_period_val->getMilliseconds();
  } else {
    logger_->log_error("Malformed delete period value, expected time format: '{}'", purge_period_str);
    purge_period_ = std::chrono::seconds{1};
  }
  const auto encrypted_env = createEncryptingEnv(utils::crypto::EncryptionManager{configuration->getHome()}, DbEncryptionOptions{directory_, ENCRYPTION_KEY_NAME});
  logger_->log_info("Using {} DatabaseContentRepository", encrypted_env ? "encrypted" : "plaintext");

  setCompactionPeriod(configuration);

  auto set_db_opts = [encrypted_env] (minifi::internal::Writable<rocksdb::DBOptions>& db_opts) {
    minifi::internal::setCommonRocksDbOptions(db_opts);
    if (encrypted_env) {
      db_opts.set(&rocksdb::DBOptions::env, encrypted_env.get(), EncryptionEq{});
    } else {
      db_opts.set(&rocksdb::DBOptions::env, rocksdb::Env::Default());
    }
  };
  auto set_cf_opts = [&configuration] (rocksdb::ColumnFamilyOptions& cf_opts) {
    cf_opts.OptimizeForPointLookup(4);
    cf_opts.merge_operator = std::make_shared<StringAppender>();
    cf_opts.max_successive_merges = 0;
    if (auto compression_type = minifi::internal::readConfiguredCompressionType(configuration, Configure::nifi_content_repository_rocksdb_compression)) {
      cf_opts.compression = *compression_type;
    }
  };
  db_ = minifi::internal::RocksDatabase::create(set_db_opts, set_cf_opts, directory_,
    minifi::internal::getRocksDbOptionsToOverride(configuration, Configure::nifi_content_repository_rocksdb_options));
  if (db_->open()) {
    logger_->log_debug("NiFi Content DB Repository database open {} success", directory_);
    is_valid_ = true;
  } else {
    logger_->log_error("NiFi Content DB Repository database open {} fail", directory_);
    is_valid_ = false;
  }

  use_synchronous_writes_ = configuration->get(Configure::nifi_content_repository_rocksdb_use_synchronous_writes).value_or("true") != "false";
  return is_valid_;
}

void DatabaseContentRepository::setCompactionPeriod(const std::shared_ptr<minifi::Configure> &configuration) {
  compaction_period_ = DEFAULT_COMPACTION_PERIOD;
  if (auto compaction_period_str = configuration->get(Configure::nifi_dbcontent_repository_rocksdb_compaction_period)) {
    if (auto compaction_period = TimePeriodValue::fromString(compaction_period_str.value())) {
      compaction_period_ = compaction_period->getMilliseconds();
      if (compaction_period_.count() == 0) {
        logger_->log_warn("Setting '{}' to 0 disables forced compaction", Configure::nifi_dbcontent_repository_rocksdb_compaction_period);
      }
    } else {
      logger_->log_error("Malformed property '{}', expected time period, using default", Configure::nifi_dbcontent_repository_rocksdb_compaction_period);
    }
  } else {
    logger_->log_debug("Using default compaction period of {}", compaction_period_);
  }
}

void DatabaseContentRepository::runCompaction() {
  do {
    if (auto opendb = db_->open()) {
      auto status = opendb->RunCompaction();
      logger_->log_trace("Compaction triggered: {}", status.ToString());
    } else {
      logger_->log_error("Failed to open database for compaction");
    }
  } while (!utils::StoppableThread::waitForStopRequest(compaction_period_));
}

void DatabaseContentRepository::start() {
  if (!db_ || !is_valid_) {
    return;
  }
  if (compaction_period_.count() != 0) {
    compaction_thread_ = std::make_unique<utils::StoppableThread>([this] {
      runCompaction();
    });
  }
  if (purge_period_.count() != 0) {
    gc_thread_ = std::make_unique<utils::StoppableThread>([this] {
      runGc();
    });
  }
}

void DatabaseContentRepository::stop() {
  if (db_) {
    auto opendb = db_->open();
    if (opendb) {
      opendb->FlushWAL(true);
    }
    compaction_thread_.reset();
    gc_thread_.reset();
  }
}

DatabaseContentRepository::Session::Session(std::shared_ptr<ContentRepository> repository, bool use_synchronous_writes)
    : BufferedContentSession(std::move(repository)),
      use_synchronous_writes_(use_synchronous_writes) {}

std::shared_ptr<ContentSession> DatabaseContentRepository::createSession() {
  return std::make_shared<Session>(sharedFromThis<ContentRepository>(), use_synchronous_writes_);
}

void DatabaseContentRepository::Session::commit() {
  auto dbContentRepository = std::static_pointer_cast<DatabaseContentRepository>(repository_);
  auto opendb = dbContentRepository->db_->open();
  if (!opendb) {
    throw Exception(REPOSITORY_EXCEPTION, "Couldn't open rocksdb database to commit content changes");
  }
  auto batch = opendb->createWriteBatch();
  for (const auto& resource : managed_resources_) {
    auto outStream = dbContentRepository->write(*resource.first, false, &batch);
    if (outStream == nullptr) {
      throw Exception(REPOSITORY_EXCEPTION, "Couldn't open the underlying resource for write: " + resource.first->getContentFullPath());
    }
    const auto size = resource.second->size();
    if (outStream->write(resource.second->getBuffer()) != size) {
      throw Exception(REPOSITORY_EXCEPTION, "Failed to write new resource: " + resource.first->getContentFullPath());
    }
  }
  for (const auto& resource : append_state_) {
    auto outStream = dbContentRepository->write(*resource.first, true, &batch);
    if (outStream == nullptr) {
      throw Exception(REPOSITORY_EXCEPTION, "Couldn't open the underlying resource for append: " + resource.first->getContentFullPath());
    }
    const auto size = resource.second.stream->size();
    if (outStream->write(resource.second.stream->getBuffer()) != size) {
      throw Exception(REPOSITORY_EXCEPTION, "Failed to append to resource: " + resource.first->getContentFullPath());
    }
  }

  rocksdb::WriteOptions options;
  options.sync = use_synchronous_writes_;
  rocksdb::Status status = opendb->Write(options, &batch);
  if (!status.ok()) {
    throw Exception(REPOSITORY_EXCEPTION, "Batch write failed: " + status.ToString());
  }

  managed_resources_.clear();
  append_state_.clear();
}

std::shared_ptr<io::BaseStream> DatabaseContentRepository::write(const minifi::ResourceClaim &claim, bool append) {
  return write(claim, append, nullptr);
}

std::shared_ptr<io::BaseStream> DatabaseContentRepository::read(const minifi::ResourceClaim &claim) {
  // the traditional approach with these has been to return -1 from the stream; however, since we have the ability here
  // we can simply return a nullptr, which is also valid from the API when this stream is not valid.
  if (!is_valid_ || !db_)
    return nullptr;
  return std::make_shared<io::RocksDbStream>(claim.getContentFullPath(), gsl::make_not_null<minifi::internal::RocksDatabase*>(db_.get()), false);
}

bool DatabaseContentRepository::exists(const minifi::ResourceClaim &streamId) {
  auto opendb = db_->open();
  if (!opendb) {
    return false;
  }
  std::string value;
  rocksdb::Status status;
  status = opendb->Get(rocksdb::ReadOptions(), streamId.getContentFullPath(), &value);
  if (status.ok()) {
    logger_->log_debug("{} exists", streamId.getContentFullPath());
    return true;
  } else {
    logger_->log_debug("{} does not exist", streamId.getContentFullPath());
    return false;
  }
}

bool DatabaseContentRepository::removeKeySync(const std::string &content_path) {
  if (!is_valid_ || !db_)
    return false;
  // synchronous deletion
  auto opendb = db_->open();
  if (!opendb) {
    return false;
  }
  rocksdb::Status status = opendb->Delete(rocksdb::WriteOptions(), content_path);
  if (status.ok()) {
    logger_->log_debug("Deleting resource {}", content_path);
    return true;
  } else if (status.IsNotFound()) {
    logger_->log_debug("Resource {} was not found", content_path);
    return true;
  } else {
    logger_->log_debug("Attempted, but could not delete {}", content_path);
    return false;
  }
}

bool DatabaseContentRepository::removeKey(const std::string& content_path) {
  if (purge_period_ == std::chrono::seconds(0)) {
    return removeKeySync(content_path);
  }
  // asynchronous deletion
  std::lock_guard guard(keys_mtx_);
  logger_->log_debug("Staging resource for deletion {}", content_path);
  keys_to_delete_.push_back(content_path);
  return true;
}

void DatabaseContentRepository::runGc() {
  while (!utils::StoppableThread::waitForStopRequest(purge_period_)) {
    auto opendb = db_->open();
    if (!opendb) {
      continue;
    }
    // keys_to_delete_ is not persisted, in memory only, and is lost on restart
    // the clearOrphans method is executed during agent startup making sure that this
    // does not cause a content leak
    std::vector<std::string> keys;
    {
      std::lock_guard guard(keys_mtx_);
      keys = std::exchange(keys_to_delete_, std::vector<std::string>{});
    }
    auto batch = opendb->createWriteBatch();
    for (auto& key : keys) {
      batch.Delete(key);
    }
    rocksdb::Status status;
    status = opendb->Write(rocksdb::WriteOptions(), &batch);
    if (status.ok()) {
      for (auto& key : keys) {
        logger_->log_debug("Deleted resource async {}", key);
      }
    } else {
      for (auto& key : keys) {
        logger_->log_debug("Failed to delete resource async {}", key);
      }
      // move keys we could not delete back to the list for a retry
      std::lock_guard guard(keys_mtx_);
      keys_to_delete_.insert(keys_to_delete_.end(), keys.begin(), keys.end());
    }
  }
}

std::shared_ptr<io::BaseStream> DatabaseContentRepository::write(const minifi::ResourceClaim& claim, bool /*append*/, minifi::internal::WriteBatch* batch) {
  // the traditional approach with these has been to return -1 from the stream; however, since we have the ability here
  // we can simply return a nullptr, which is also valid from the API when this stream is not valid.
  if (!is_valid_ || !db_)
    return nullptr;
  // append is already supported in all modes
  return std::make_shared<io::RocksDbStream>(claim.getContentFullPath(), gsl::make_not_null<minifi::internal::RocksDatabase*>(db_.get()), true, batch);
}

void DatabaseContentRepository::clearOrphans() {
  if (!is_valid_ || !db_) {
    logger_->log_error("Cannot delete orphan content entries, repository is invalid");
    return;
  }
  auto opendb = db_->open();
  if (!opendb) {
    logger_->log_error("Cannot delete orphan content entries, could not open repository");
    return;
  }
  std::vector<std::string> keys_to_be_deleted;
  auto it = opendb->NewIterator(rocksdb::ReadOptions());
  for (it->SeekToFirst(); it->Valid(); it->Next()) {
    auto key = it->key().ToString();
    std::lock_guard<std::mutex> lock(count_map_mutex_);
    auto claim_it = count_map_.find(key);
    if (claim_it == count_map_.end() || claim_it->second == 0) {
      logger_->log_error("Deleting orphan resource {}", key);
      keys_to_be_deleted.push_back(key);
    }
  }
  auto batch = opendb->createWriteBatch();
  for (auto& key : keys_to_be_deleted) {
    batch.Delete(key);
  }

  rocksdb::Status status = opendb->Write(rocksdb::WriteOptions(), &batch);

  if (!status.ok()) {
    logger_->log_error("Could not delete orphan contents from rocksdb database: {}", status.ToString());
    std::lock_guard<std::mutex> lock(purge_list_mutex_);
    for (const auto& key : keys_to_be_deleted) {
      purge_list_.push_back(key);
    }
  }
}

uint64_t DatabaseContentRepository::getRepositorySize() const {
  return (utils::optional_from_ptr(db_.get()) |
          utils::andThen([](const auto& db) { return db->open(); }) |
          utils::andThen([](const auto& opendb) { return opendb.getApproximateSizes(); })).value_or(0);
}

uint64_t DatabaseContentRepository::getRepositoryEntryCount() const {
  return (utils::optional_from_ptr(db_.get()) |
          utils::andThen([](const auto& db) { return db->open(); }) |
          utils::andThen([](auto&& opendb) -> std::optional<uint64_t> {
              std::string key_count;
              opendb.GetProperty("rocksdb.estimate-num-keys", &key_count);
              if (!key_count.empty()) {
                return std::stoull(key_count);
              }
              return std::nullopt;
            })).value_or(0);
}

std::optional<RepositoryMetricsSource::RocksDbStats> DatabaseContentRepository::getRocksDbStats() const {
  auto opendb = db_->open();
  if (!opendb) {
    return RocksDbStats{};
  }

  return opendb->getStats();
}

REGISTER_RESOURCE_AS(DatabaseContentRepository, InternalResource, ("DatabaseContentRepository", "databasecontentrepository"));

}  // namespace org::apache::nifi::minifi::core::repository
