/**
 *
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
#include "FlowFileRecord.h"
#include "FlowFileRepository.h"

#include "rocksdb/options.h"
#include "rocksdb/write_batch.h"
#include "rocksdb/slice.h"

#include <chrono>
#include <memory>
#include <string>
#include <utility>
#include <vector>
#include <list>

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace core {
namespace repository {

void FlowFileRepository::flush() {
  auto opendb = db_->open();
  if (!opendb) {
    return;
  }
  rocksdb::WriteBatch batch;
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

  for(size_t i=0; i<keys.size() && i<values.size() && i<multistatus.size(); ++i) {
    if(!multistatus[i].ok()) {
      logger_->log_error("Failed to read key from rocksdb: %s! DB is most probably in an inconsistent state!", keys[i].data());
      keystrings.remove(keys[i].data());
      continue;
    }

    utils::Identifier containerId;
    auto eventRead = FlowFileRecord::DeSerialize(reinterpret_cast<const uint8_t *>(values[i].data()), values[i].size(), content_repo_, containerId);
    if (eventRead) {
      purgeList.push_back(eventRead);
    }
    logger_->log_debug("Issuing batch delete, including %s, Content path %s", eventRead->getUUIDStr(), eventRead->getContentFullPath());
    batch.Delete(keys[i]);
  }

  auto operation = [&batch, &opendb]() { return opendb->Write(rocksdb::WriteOptions(), &batch); };

  if (!ExecuteWithRetry(operation)) {
    for (const auto& key: keystrings) {
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
  if (running_) {
    prune_stored_flowfiles();
  }
  while (running_) {
    std::this_thread::sleep_for(std::chrono::milliseconds(purge_period_));
    flush();
    auto now = std::chrono::steady_clock::now();
    if ((now-last) > std::chrono::seconds(30)) {
      printStats();
      last = now;
    }
  }
  flush();
}

void FlowFileRepository::prune_stored_flowfiles() {
  rocksdb::Options options;
  options.create_if_missing = true;
  options.use_direct_io_for_flush_and_compaction = true;
  options.use_direct_reads = true;
  minifi::internal::RocksDatabase checkpointDB(options, checkpoint_dir_, minifi::internal::RocksDatabase::Mode::ReadOnly);
  utils::optional<minifi::internal::OpenRocksDB> opendb;
  if (nullptr != checkpoint_) {
    opendb = checkpointDB.open();
    if (!opendb) {
      opendb = db_->open();
    }
    if (!opendb) {
      logger_->log_trace("Could not open neither the checkpoint nor the live database.");
      return;
    }
  } else {
    logger_->log_trace("Could not open checkpoint as object doesn't exist. Likely not needed or file system error.");
    return;
  }

  auto it = opendb->NewIterator(rocksdb::ReadOptions());
  for (it->SeekToFirst(); it->Valid(); it->Next()) {
    utils::Identifier containerId;
    auto eventRead = FlowFileRecord::DeSerialize(reinterpret_cast<const uint8_t *>(it->value().data()), it->value().size(), content_repo_, containerId);
    std::string key = it->key().ToString();
    if (eventRead) {
      // on behalf of the just resurrected persisted instance
      auto claim = eventRead->getResourceClaim();
      if (claim) claim->increaseFlowFileRecordOwnedCount();
      bool found = false;
      auto search = containers.find(containerId.to_string());
      found = (search != containers.end());
      if (!found) {
        // for backward compatibility
        search = connectionMap.find(containerId.to_string());
        found = (search != connectionMap.end());
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
}

bool FlowFileRepository::ExecuteWithRetry(std::function<rocksdb::Status()> operation) {
  int waitTime = 0;
  for (int i=0; i<3; ++i) {
    auto status = operation();
    if (status.ok()) {
      logger_->log_trace("Rocksdb operation executed successfully");
      return true;
    }
    logger_->log_error("Rocksdb operation failed: %s", status.ToString());
    waitTime += FLOWFILE_REPOSITORY_RETRY_INTERVAL_INCREMENTS;
    std::this_thread::sleep_for(std::chrono::milliseconds(waitTime));
  }
  return false;
}

/**
 * Returns True if there is data to interrogate.
 * @return true if our db has data stored.
 */
bool FlowFileRepository::need_checkpoint(minifi::internal::OpenRocksDB& opendb){
  auto it = opendb.NewIterator(rocksdb::ReadOptions());
  it->SeekToFirst();
  return it->Valid();
}
void FlowFileRepository::initialize_repository() {
  auto opendb = db_->open();
  if (!opendb) {
    logger_->log_trace("Couldn't open database, no way to checkpoint");
    return;
  }
  // first we need to establish a checkpoint iff it is needed.
  if (!need_checkpoint(*opendb)){
    logger_->log_trace("Do not need checkpoint");
    return;
  }
  rocksdb::Checkpoint *checkpoint;
  // delete any previous copy
  if (utils::file::FileUtils::delete_dir(checkpoint_dir_) >= 0 && opendb->NewCheckpoint(&checkpoint).ok()) {
    if (checkpoint->CreateCheckpoint(checkpoint_dir_).ok()) {
      checkpoint_ = std::unique_ptr<rocksdb::Checkpoint>(checkpoint);
      logger_->log_trace("Created checkpoint directory");
    } else {
      logger_->log_trace("Could not create checkpoint. Corrupt?");
    }
  } else
    logger_->log_trace("Could not create checkpoint directory. Not properly deleted?");
}

void FlowFileRepository::loadComponent(const std::shared_ptr<core::ContentRepository> &content_repo) {
  content_repo_ = content_repo;
  repo_size_ = 0;

  initialize_repository();
}

} /* namespace repository */
} /* namespace core */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
