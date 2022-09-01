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

#ifndef LIBMINIFI_INCLUDE_CORE_REPOSITORY_VOLATILECONTENTREPOSITORY_H_
#define LIBMINIFI_INCLUDE_CORE_REPOSITORY_VOLATILECONTENTREPOSITORY_H_

#include <map>
#include <memory>
#include <string>

#include "core/Core.h"
#include "AtomicRepoEntries.h"
#include "io/AtomicEntryStream.h"
#include "../ContentRepository.h"
#include "core/repository/VolatileRepository.h"
#include "properties/Configure.h"
#include "core/Connectable.h"
#include "core/logging/LoggerFactory.h"
#include "utils/GeneralUtils.h"

namespace org::apache::nifi::minifi::core::repository {

/**
 * Purpose: Stages content into a volatile area of memory. Note that when the maximum number
 * of entries is consumed we will rollback a session to wait for others to be freed.
 */
class VolatileContentRepository : public core::ContentRepository, public core::repository::VolatileRepository<ResourceClaim::Path> {
 public:
  static const char *minimal_locking;

  explicit VolatileContentRepository(std::string name = getClassName<VolatileContentRepository>())
      : core::SerializableComponent(name),
        core::repository::VolatileRepository<ResourceClaim::Path>(name),
        minimize_locking_(true),
        logger_(logging::LoggerFactory<VolatileContentRepository>::getLogger()) {
    max_count_ = 15000;
  }

  ~VolatileContentRepository() override {
    logger_->log_debug("Clearing repository");
    if (!minimize_locking_) {
      std::lock_guard<std::mutex> lock(map_mutex_);
      for (const auto &item : master_list_) {
        delete item.second;
      }
      master_list_.clear();
    }
  }

  bool start() override {
    return true;
  }

  bool stop() override {
    return true;
  }

  /**
   * Initialize the volatile content repo
   * @param configure configuration
   */
  bool initialize(const std::shared_ptr<Configure> &configure) override;

  /**
   * Creates writable stream.
   * @param claim resource claim
   * @return BaseStream shared pointer that represents the stream the consumer will write to.
   */
  std::shared_ptr<io::BaseStream> write(const minifi::ResourceClaim &claim, bool append) override;

  /**
   * Creates readable stream.
   * @param claim resource claim
   * @return BaseStream shared pointer that represents the stream from which the consumer will read..
   */
  std::shared_ptr<io::BaseStream> read(const minifi::ResourceClaim &claim) override;

  bool exists(const minifi::ResourceClaim &claim) override;

  /**
   * Closes the claim.
   * @return whether or not the claim is associated with content stored in volatile memory.
   */
  bool close(const minifi::ResourceClaim &claim) override {
    return remove(claim);
  }

  /**
   * Closes the claim.
   * @return whether or not the claim is associated with content stored in volatile memory.
   */
  bool remove(const minifi::ResourceClaim &claim) override;

 private:
  bool minimize_locking_;

  // mutex and master list that represent a cache of Atomic entries. this exists so that we don't have to walk the atomic entry list.
  // The idea is to reduce the computational complexity while keeping access as maximally lock free as we can.
  std::mutex map_mutex_;

  std::map<ResourceClaim::Path, AtomicEntry<ResourceClaim::Path>*> master_list_;

  // logger
  std::shared_ptr<logging::Logger> logger_;
};

}  // namespace org::apache::nifi::minifi::core::repository

#endif  // LIBMINIFI_INCLUDE_CORE_REPOSITORY_VOLATILECONTENTREPOSITORY_H_
