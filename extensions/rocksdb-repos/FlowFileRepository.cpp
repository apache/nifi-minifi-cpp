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
#include "FlowFileRepository.h"
#include "rocksdb/write_batch.h"
#include <memory>
#include <string>
#include <utility>
#include <vector>
#include "FlowFileRecord.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace core {
namespace repository {

void FlowFileRepository::flush() {
  rocksdb::WriteBatch batch;
  std::string key;
  std::string value;
  rocksdb::ReadOptions options;

  std::vector<std::shared_ptr<FlowFileRecord>> purgeList;

  uint64_t decrement_total = 0;
  while (keys_to_delete.size_approx() > 0) {
    if (keys_to_delete.try_dequeue(key)) {
      db_->Get(options, key, &value);
      decrement_total += value.size();
      std::shared_ptr<FlowFileRecord> eventRead = std::make_shared<FlowFileRecord>(shared_from_this(), content_repo_);
      if (eventRead->DeSerialize(reinterpret_cast<const uint8_t *>(value.data()), value.size())) {
        purgeList.push_back(eventRead);
      }
      logger_->log_debug("Issuing batch delete, including %s, Content path %s", eventRead->getUUIDStr(), eventRead->getContentFullPath());
      batch.Delete(key);
    }
  }
  if (db_->Write(rocksdb::WriteOptions(), &batch).ok()) {
    logger_->log_trace("Decrementing %u from a repo size of %u", decrement_total, repo_size_.load());
    if (decrement_total > repo_size_.load()) {
      repo_size_ = 0;
    } else {
      repo_size_ -= decrement_total;
    }
  }

  if (nullptr != content_repo_) {
    for (const auto &ffr : purgeList) {
      auto claim = ffr->getResourceClaim();
      if (claim != nullptr) {
        content_repo_->removeIfOrphaned(claim);
      }
    }
  }
}

void FlowFileRepository::run() {
  // threshold for purge

  if (running_) {
    prune_stored_flowfiles();
  }
  while (running_) {
    std::this_thread::sleep_for(std::chrono::milliseconds(purge_period_));

    flush();

    uint64_t size = getRepoSize();

    if (size > (uint64_t) max_partition_bytes_)
      repo_full_ = true;
    else
      repo_full_ = false;
  }
}

void FlowFileRepository::prune_stored_flowfiles() {
  rocksdb::DB* stored_database_;
  bool corrupt_checkpoint = false;
  if (nullptr != checkpoint_) {
    rocksdb::Options options;
    options.create_if_missing = true;
    options.use_direct_io_for_flush_and_compaction = true;
    options.use_direct_reads = true;
    rocksdb::Status status = rocksdb::DB::OpenForReadOnly(options, FLOWFILE_CHECKPOINT_DIRECTORY, &stored_database_);
    if (!status.ok()) {
      stored_database_ = db_;
    }
  } else {
    logger_->log_trace("Could not open checkpoint as object doesn't exist. Likely not needed or file system error.");
    return;
  }

  rocksdb::Iterator* it = stored_database_->NewIterator(rocksdb::ReadOptions());
  for (it->SeekToFirst(); it->Valid(); it->Next()) {
    std::shared_ptr<FlowFileRecord> eventRead = std::make_shared<FlowFileRecord>(shared_from_this(), content_repo_);
    std::string key = it->key().ToString();
    repo_size_ += it->value().size();
    if (eventRead->DeSerialize(reinterpret_cast<const uint8_t *>(it->value().data()), it->value().size())) {
      logger_->log_debug("Found connection for %s, path %s ", eventRead->getConnectionUuid(), eventRead->getContentFullPath());
      auto search = connectionMap.find(eventRead->getConnectionUuid());
      if (!corrupt_checkpoint && search != connectionMap.end()) {
        // we find the connection for the persistent flowfile, create the flowfile and enqueue that
        std::shared_ptr<core::FlowFile> flow_file_ref = std::static_pointer_cast<core::FlowFile>(eventRead);
        eventRead->setStoredToRepository(true);
        search->second->put(eventRead);
      } else {
        logger_->log_warn("Could not find connection for %s, path %s ", eventRead->getConnectionUuid(), eventRead->getContentFullPath());
        if (eventRead->getContentFullPath().length() > 0) {
          if (nullptr != eventRead->getResourceClaim()) {
            content_repo_->remove(eventRead->getResourceClaim());
          }
        }
        keys_to_delete.enqueue(key);
      }
    } else {
      keys_to_delete.enqueue(key);
    }
  }

  delete it;

}

/**
 * Returns True if there is data to interrogate.
 * @return true if our db has data stored.
 */
bool FlowFileRepository::need_checkpoint(){
  std::unique_ptr<rocksdb::Iterator> it = std::unique_ptr<rocksdb::Iterator>(db_->NewIterator(rocksdb::ReadOptions()));
  for (it->SeekToFirst(); it->Valid(); it->Next()) {
    return true;
  }
  return false;
}
void FlowFileRepository::initialize_repository() {
  // first we need to establish a checkpoint iff it is needed.
  if (!need_checkpoint()){
    logger_->log_trace("Do not need checkpoint");
    return;
  }
  rocksdb::Checkpoint *checkpoint;
  // delete any previous copy
  if (utils::file::FileUtils::delete_dir(FLOWFILE_CHECKPOINT_DIRECTORY) >= 0 && rocksdb::Checkpoint::Create(db_, &checkpoint).ok()) {
    if (checkpoint->CreateCheckpoint(FLOWFILE_CHECKPOINT_DIRECTORY).ok()) {
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
