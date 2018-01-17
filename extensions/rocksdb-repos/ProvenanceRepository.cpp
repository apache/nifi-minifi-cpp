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

#include "ProvenanceRepository.h"
#include "rocksdb/write_batch.h"
#include <string>
#include <vector>
#include "rocksdb/options.h"
#include "provenance/Provenance.h"
namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace provenance {

void ProvenanceRepository::flush() {
  rocksdb::WriteBatch batch;
  std::string key;
  std::string value;
  rocksdb::ReadOptions options;
  uint64_t decrement_total = 0;
  while (keys_to_delete.size_approx() > 0) {
    if (keys_to_delete.try_dequeue(key)) {
      db_->Get(options, key, &value);
      decrement_total += value.size();
      batch.Delete(key);
      logger_->log_debug("Removing %s", key);
    }
  }
  if (db_->Write(rocksdb::WriteOptions(), &batch).ok()) {
    logger_->log_debug("Decrementing %u from a repo size of %u", decrement_total, repo_size_.load());
    if (decrement_total > repo_size_.load()) {
      repo_size_ = 0;
    } else {
      repo_size_ -= decrement_total;
    }
  }
}

void ProvenanceRepository::run() {
  while (running_) {
    std::this_thread::sleep_for(std::chrono::milliseconds(purge_period_));
    uint64_t curTime = getTimeMillis();
    // threshold for purge
    uint64_t purgeThreshold = max_partition_bytes_ * 3 / 4;

    uint64_t size = getRepoSize();

    if (size >= purgeThreshold) {
      rocksdb::Iterator* it = db_->NewIterator(rocksdb::ReadOptions());
      for (it->SeekToFirst(); it->Valid(); it->Next()) {
        ProvenanceEventRecord eventRead;
        std::string key = it->key().ToString();
        uint64_t eventTime = eventRead.getEventTime(reinterpret_cast<uint8_t*>(const_cast<char*>(it->value().data())), it->value().size());
        if (eventTime > 0) {
          if ((curTime - eventTime) > (uint64_t)max_partition_millis_)
            Delete(key);
        } else {
          logger_->log_debug("NiFi Provenance retrieve event %s fail", key);
          Delete(key);
        }
      }
      delete it;
    }
    flush();
    size = getRepoSize();
    if (size > (uint64_t)max_partition_bytes_)
      repo_full_ = true;
    else
      repo_full_ = false;
  }
}
} /* namespace provenance */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

