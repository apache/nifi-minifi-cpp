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

#include "provenance/ProvenanceRepository.h"
#include <string>
#include <vector>
#include "provenance/Provenance.h"
namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace provenance {

void ProvenanceRepository::run() {
  // threshold for purge
  uint64_t purgeThreshold = max_partition_bytes_ * 3 / 4;
  while (running_) {
    std::this_thread::sleep_for(std::chrono::milliseconds(purge_period_));
    std::this_thread::sleep_for(std::chrono::milliseconds(purge_period_));
    uint64_t curTime = getTimeMillis();
    uint64_t size = repoSize();
    if (size >= purgeThreshold) {
      std::vector<std::string> purgeList;
      leveldb::Iterator* it = db_->NewIterator(leveldb::ReadOptions());
      for (it->SeekToFirst(); it->Valid(); it->Next()) {
        ProvenanceEventRecord eventRead;
        std::string key = it->key().ToString();
        if (eventRead.DeSerialize(
            reinterpret_cast<uint8_t*>(const_cast<char*>(it->value().data())),
            it->value().size())) {
          if ((curTime - eventRead.getEventTime()) > max_partition_millis_)
            purgeList.push_back(key);
        } else {
          logger_->log_debug("NiFi Provenance retrieve event %s fail",
                             key.c_str());
          purgeList.push_back(key);
        }
      }
      delete it;
      std::vector<std::string>::iterator itPurge;

      for (itPurge = purgeList.begin(); itPurge != purgeList.end(); itPurge++) {
        std::string eventId = *itPurge;
        logger_->log_info("ProvenanceRepository Repo Purge %s",
                          eventId.c_str());
        Delete(eventId);
      }
    }
    if (size > max_partition_bytes_)
      repo_full_ = true;
    else
      repo_full_ = false;
  }
  return;
}
} /* namespace provenance */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

