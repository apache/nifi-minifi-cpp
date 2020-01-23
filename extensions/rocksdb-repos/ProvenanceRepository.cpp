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
namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace provenance {

void ProvenanceRepository::printStats() {
  logger_->log_info("ProvenanceRepository stats:");

  std::string out;
  db_->GetProperty("rocksdb.estimate-num-keys", &out);
  logger_->log_info("\\--Estimated key count: %s", out);

  db_->GetProperty("rocksdb.estimate-table-readers-mem", &out);
  logger_->log_info("\\--Estimated table readers memory consumption: %s", out);

  db_->GetProperty("rocksdb.cur-size-all-mem-tables", &out);
  logger_->log_info("\\--Size of all memory tables: %s", out);
}

void ProvenanceRepository::run() {
  while (running_) {
    std::this_thread::sleep_for(std::chrono::seconds(10));
    printStats();
  }
}
} /* namespace provenance */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

