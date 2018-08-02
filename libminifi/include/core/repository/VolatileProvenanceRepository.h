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
#ifndef LIBMINIFI_INCLUDE_CORE_REPOSITORY_VOLATILEPROVENANCEREPOSITORY_H_
#define LIBMINIFI_INCLUDE_CORE_REPOSITORY_VOLATILEPROVENANCEREPOSITORY_H_

#include "VolatileRepository.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace core {
namespace repository {

/**
 * Volatile provenance repository.
 */
class VolatileProvenanceRepository : public VolatileRepository<std::string> {

 public:
  explicit VolatileProvenanceRepository(std::string repo_name = "", std::string dir = REPOSITORY_DIRECTORY, int64_t maxPartitionMillis = MAX_REPOSITORY_ENTRY_LIFE_TIME, int64_t maxPartitionBytes =
  MAX_REPOSITORY_STORAGE_SIZE,
                                        uint64_t purgePeriod = REPOSITORY_PURGE_PERIOD)
      : core::SerializableComponent(repo_name),VolatileRepository(repo_name.length() > 0 ? repo_name : core::getClassName<VolatileRepository>(), "", maxPartitionMillis, maxPartitionBytes, purgePeriod)

  {
    purge_required_ = false;
  }

  virtual void run() {
    repo_full_ = false;
  }
 protected:
  virtual void emplace(RepoValue<std::string> &old_value) {
    purge_list_.push_back(old_value.getKey());
  }
 private:

};

} /* namespace repository */
} /* namespace core */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

#endif /* LIBMINIFI_INCLUDE_CORE_REPOSITORY_VOLATILEPROVENANCEREPOSITORY_H_ */
