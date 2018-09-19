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
#ifndef LIBMINIFI_INCLUDE_CORE_REPOSITORY_VolatileContentRepository_H_
#define LIBMINIFI_INCLUDE_CORE_REPOSITORY_VolatileContentRepository_H_

#include "core/Core.h"
#include "AtomicRepoEntries.h"
#include "io/AtomicEntryStream.h"
#include "../ContentRepository.h"
#include "core/repository/VolatileRepository.h"
#include "properties/Configure.h"
#include "core/Connectable.h"
#include "core/logging/LoggerConfiguration.h"
namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace core {
namespace repository {

/**
 * Purpose: Stages content into a volatile area of memory. Note that   when the maximum number
 * of entries is consumed we will rollback a session to wait for others to be freed.
 */
class VolatileContentRepository : public core::ContentRepository, public virtual core::repository::VolatileRepository<std::shared_ptr<minifi::ResourceClaim>> {
 public:

  static const char *minimal_locking;

  explicit VolatileContentRepository(std::string name = getClassName<VolatileContentRepository>())
      : core::SerializableComponent(name),
        core::repository::VolatileRepository<std::shared_ptr<minifi::ResourceClaim>>(name),
        minimize_locking_(true),
        logger_(logging::LoggerFactory<VolatileContentRepository>::getLogger()) {
    max_count_ = 15000;
  }
  virtual ~VolatileContentRepository() {
    logger_->log_debug("Clearing repository");
    if (!minimize_locking_) {
      std::lock_guard<std::mutex> lock(map_mutex_);
      for (const auto &item : master_list_) {
        delete item.second;
      }
      master_list_.clear();
    }

  }

  /**
   * Initialize the volatile content repo
   * @param configure configuration
   */
  virtual bool initialize(const std::shared_ptr<Configure> &configure);

  /**
   * Stop any thread associated with the volatile content repository.
   */
  virtual void stop();

  /**
   * Creates writable stream.
   * @param claim resource claim
   * @return BaseStream shared pointer that represents the stream the consumer will write to.
   */
  virtual std::shared_ptr<io::BaseStream> write(const std::shared_ptr<minifi::ResourceClaim> &claim);

  /**
   * Creates readable stream.
   * @param claim resource claim
   * @return BaseStream shared pointer that represents the stream from which the consumer will read..
   */
  virtual std::shared_ptr<io::BaseStream> read(const std::shared_ptr<minifi::ResourceClaim> &claim);

  virtual bool exists(const std::shared_ptr<minifi::ResourceClaim> &streamId);

  /**
   * Closes the claim.
   * @return whether or not the claim is associated with content stored in volatile memory.
   */
  virtual bool close(const std::shared_ptr<minifi::ResourceClaim> &claim) {
    return remove(claim);
  }

  /**
   * Closes the claim.
   * @return whether or not the claim is associated with content stored in volatile memory.
   */
  virtual bool remove(const std::shared_ptr<minifi::ResourceClaim> &claim);

 protected:

  virtual void start();

  virtual void run();

  template<typename T2>
  std::shared_ptr<T2> shared_from_parent() {
    return std::dynamic_pointer_cast<T2>(shared_from_this());
  }

 private:

  bool minimize_locking_;

  // function pointers that are associated with the claims.
  std::function<bool(std::shared_ptr<minifi::ResourceClaim>, std::shared_ptr<minifi::ResourceClaim>)> resource_claim_comparator_;
  std::function<bool(std::shared_ptr<minifi::ResourceClaim>)> resource_claim_check_;
  std::function<void(std::shared_ptr<minifi::ResourceClaim>)> claim_reclaimer_;

  // mutex and master list that represent a cache of Atomic entries. this exists so that we don't have to walk the atomic entry list.
  // The idea is to reduce the computational complexity while keeping access as maximally lock free as we can.
  std::mutex map_mutex_;

  std::map<std::string, AtomicEntry<std::shared_ptr<minifi::ResourceClaim>>*> master_list_;

  // logger
  std::shared_ptr<logging::Logger> logger_;
};

} /* namespace repository */
} /* namespace core */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

#endif /* LIBMINIFI_INCLUDE_CORE_REPOSITORY_VolatileContentRepository_H_ */
