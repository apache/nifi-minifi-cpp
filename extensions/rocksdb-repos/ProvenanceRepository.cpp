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

#include <string>

#include "core/Resource.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace provenance {

void ProvenanceRepository::printStats() {
  std::string key_count;
  db_->GetProperty("rocksdb.estimate-num-keys", &key_count);

  std::string table_readers;
  db_->GetProperty("rocksdb.estimate-table-readers-mem", &table_readers);

  std::string all_memtables;
  db_->GetProperty("rocksdb.cur-size-all-mem-tables", &all_memtables);

  logger_->log_info("Repository stats: key count: %s, table readers size: %s, all memory tables size: %s",
                    key_count, table_readers, all_memtables);
}

void ProvenanceRepository::run() {
  size_t count = 0;
  while (running_) {
    std::this_thread::sleep_for(std::chrono::seconds(1));
    count++;
    // Hack, to be removed in scope of https://issues.apache.org/jira/browse/MINIFICPP-1145
    count = count % 30;
    if (count == 0) {
      printStats();
    }
  }
}

REGISTER_INTERNAL_RESOURCE_AS(ProvenanceRepository, ("ProvenanceRepository", "provenancerepository"));

} /* namespace provenance */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

