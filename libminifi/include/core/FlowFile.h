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
#ifndef LIBMINIFI_INCLUDE_CORE_FLOWFILE_H_
#define LIBMINIFI_INCLUDE_CORE_FLOWFILE_H_

#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>

#include "utils/TimeUtil.h"
#include "ResourceClaim.h"
#include "Connectable.h"
#include "WeakReference.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace core {

class FlowFile : public core::Connectable, public ReferenceContainer {
 private:
  class FlowFileOwnedResourceClaimPtr{
   public:
    FlowFileOwnedResourceClaimPtr() = default;
    explicit FlowFileOwnedResourceClaimPtr(const std::shared_ptr<ResourceClaim>& claim) : claim_(claim) {
      if (claim_) claim_->increaseFlowFileRecordOwnedCount();
    }
    explicit FlowFileOwnedResourceClaimPtr(std::shared_ptr<ResourceClaim>&& claim) : claim_(std::move(claim)) {
      if (claim_) claim_->increaseFlowFileRecordOwnedCount();
    }
    FlowFileOwnedResourceClaimPtr(const FlowFileOwnedResourceClaimPtr& ref) : claim_(ref.claim_) {
      if (claim_) claim_->increaseFlowFileRecordOwnedCount();
    }
    FlowFileOwnedResourceClaimPtr(FlowFileOwnedResourceClaimPtr&& ref) : claim_(std::move(ref.claim_)) {
      // taking ownership of claim, no need to increment/decrement
    }
    FlowFileOwnedResourceClaimPtr& operator=(const FlowFileOwnedResourceClaimPtr& ref) = delete;
    FlowFileOwnedResourceClaimPtr& operator=(FlowFileOwnedResourceClaimPtr&& ref) = delete;

    FlowFileOwnedResourceClaimPtr& set(FlowFile& owner, const FlowFileOwnedResourceClaimPtr& ref) {
      return set(owner, ref.claim_);
    }
    FlowFileOwnedResourceClaimPtr& set(FlowFile& owner, const std::shared_ptr<ResourceClaim>& newClaim) {
      auto oldClaim = claim_;
      claim_ = newClaim;
      // the order of increase/release is important
      // with refcount manipulation we should always increment first, then decrement as this way we don't accidentally
      // discard the object under ourselves, note that an equality check will not suffice as two ResourceClaim
      // instances can reference the same file (they could have the same contentPath)
      if (claim_) claim_->increaseFlowFileRecordOwnedCount();
      if (oldClaim) owner.releaseClaim(oldClaim);
      return *this;
    }
    const std::shared_ptr<ResourceClaim>& get() const {
      return claim_;
    }
    const std::shared_ptr<ResourceClaim>& operator->() const {
      return claim_;
    }
    operator bool() const noexcept {
      return static_cast<bool>(claim_);
    }
    ~FlowFileOwnedResourceClaimPtr() {
      // allow the owner FlowFile to manually release the claim
      // while logging stuff and removing it from repositories
      assert(!claim_);
    }

   private:
    /*
     * We are aiming for the constraint that all FlowFiles should have a non-null claim pointer,
     * unfortunately, for now, some places (e.g. ProcessSession::create) violate this constraint.
     * We should indicate an empty or invalid content with special claims like
     * InvalidResourceClaim and EmptyResourceClaim.
     */
    std::shared_ptr<ResourceClaim> claim_;
  };

 public:
  FlowFile();
  ~FlowFile() override;
  FlowFile& operator=(const FlowFile& other);

  /**
   * Returns a pointer to this flow file record's
   * claim
   */
  std::shared_ptr<ResourceClaim> getResourceClaim();
  /**
   * Sets _claim to the inbound claim argument
   */
  void setResourceClaim(const std::shared_ptr<ResourceClaim>& claim);

  /**
   * clear the resource claim
   */
  void clearResourceClaim();

  /**
   * Returns a pointer to this flow file record's
   * claim at the given stash key
   */
  std::shared_ptr<ResourceClaim> getStashClaim(const std::string& key);

  /**
   * Sets the given stash key to the inbound claim argument
   */
  void setStashClaim(const std::string& key, const std::shared_ptr<ResourceClaim>& claim);

  /**
   * Clear the resource claim at the given stash key
   */
  void clearStashClaim(const std::string& key);

  /**
   * Return true if the given stash claim exists
   */
  bool hasStashClaim(const std::string& key);

  /**
   * Decrease the flow file record owned count for the resource claim and, if 
   * its counter is at zero, remove it from the repo.
   */
  virtual void releaseClaim(const std::shared_ptr<ResourceClaim> claim) = 0;

  /**
   * Get lineage identifiers
   */
  std::set<std::string>& getlineageIdentifiers();

  /**
   * Returns whether or not this flow file record
   * is marked as deleted.
   * @return marked deleted
   */
  bool isDeleted() const;

  /**
   * Sets whether to mark this flow file record
   * as deleted
   * @param deleted deleted flag
   */
  void setDeleted(bool deleted);

  /**
   * Get entry date for this record
   * @return entry date uint64_t
   */
  uint64_t getEntryDate() const;

  /**
   * Gets the event time.
   * @return event time.
   */
  uint64_t getEventTime() const;
  /**
   * Get lineage start date
   * @return lineage start date uint64_t
   */
  uint64_t getlineageStartDate() const;

  /**
   * Sets the lineage start date
   * @param date new lineage start date
   */
  void setLineageStartDate(const uint64_t date);

  void setLineageIdentifiers(std::set<std::string> lineage_Identifiers) {
    lineage_Identifiers_ = lineage_Identifiers;
  }
  /**
   * Obtains an attribute if it exists. If it does the value is
   * copied into value
   * @param key key to look for
   * @param value value to set
   * @return result of finding key
   */
  bool getAttribute(std::string key, std::string& value) const;

  /**
   * Updates the value in the attribute map that corresponds
   * to key
   * @param key attribute name
   * @param value value to set to attribute name
   * @return result of finding key
   */
  bool updateAttribute(std::string key, std::string value);

  /**
   * Removes the attribute
   * @param key attribute name to remove
   * @return result of finding key
   */
  bool removeAttribute(std::string key);

  /**
   * setAttribute, if attribute already there, update it, else, add it
   */
  void setAttribute(const std::string& key, const std::string& value) {
    attributes_[key] = value;
  }

  /**
   * Returns the map of attributes
   * @return attributes.
   */
  std::map<std::string, std::string> getAttributes() const {
    return attributes_;
  }

  /**
   * Returns the map of attributes
   * @return attributes.
   */
  std::map<std::string, std::string> *getAttributesPtr() {
    return &attributes_;
  }

  /**
   * adds an attribute if it does not exist
   *
   */
  bool addAttribute(const std::string& key, const std::string& value);

  /**
   * Set the size of this record.
   * @param size size of record to set.Ï
   */
  void setSize(const uint64_t size) {
    size_ = size;
  }
  /**
   * Returns the size of corresponding flow file
   * @return size as a uint64_t
   */
  uint64_t getSize() const;

  /**
   * Sets the offset
   * @param offset offset to apply to this record.
   */
  void setOffset(const uint64_t offset) {
    offset_ = offset;
  }

  /**
   * Sets the penalty expiration
   * @param penaltyExp new penalty expiration
   */
  void setPenaltyExpiration(const uint64_t penaltyExp) {
    penaltyExpiration_ms_ = penaltyExp;
  }

  uint64_t getPenaltyExpiration() const {
    return penaltyExpiration_ms_;
  }

  /**
   * Gets the offset within the flow file
   * @return size as a uint64_t
   */
  uint64_t getOffset() const;

  bool getUUID(utils::Identifier& other) {
    other = uuid_;
    return true;
  }

  // Check whether it is still being penalized
  bool isPenalized() const {
    return (penaltyExpiration_ms_ > 0 ? penaltyExpiration_ms_ > getTimeMillis() : false);
  }

  uint64_t getId() const {
    return id_;
  }

  /**
   * Yield
   */
  void yield() override {
  }
  /**
   * Determines if we are connected and operating
   */
  bool isRunning() override {
    return true;
  }

  /**
   * Determines if work is available by this connectable
   * @return boolean if work is available.
   */
  bool isWorkAvailable() override {
    return true;
  }

  /**
   * Sets the original connection with a shared pointer.
   * @param connection shared connection.
   */
  void setConnection(std::shared_ptr<core::Connectable>& connection);

  /**
   * Sets the original connection with a shared pointer.
   * @param connection shared connection.
   */
  void setConnection(std::shared_ptr<core::Connectable>&& connection);

  /**
   * Returns the connection referenced by this record.
   * @return shared connection pointer.
   */
  std::shared_ptr<core::Connectable> getConnection() const;
  /**
   * Sets the original connection with a shared pointer.
   * @param connection shared connection.
   */
  void setOriginalConnection(std::shared_ptr<core::Connectable>& connection);
  /**
   * Returns the original connection referenced by this record.
   * @return shared original connection pointer.
   */
  std::shared_ptr<core::Connectable> getOriginalConnection() const;

  void setStoredToRepository(bool storedInRepository) {
    stored = storedInRepository;
  }

  bool isStored() const {
    return stored;
  }

 protected:
  bool stored;
  // Mark for deletion
  bool marked_delete_;
  // Date at which the flow file entered the flow
  uint64_t entry_date_;
  // event time
  uint64_t event_time_;
  // Date at which the origin of this flow file entered the flow
  uint64_t lineage_start_date_;
  // Date at which the flow file was queued
  uint64_t last_queue_date_;
  // Size in bytes of the data corresponding to this flow file
  uint64_t size_;
  // A global unique identifier
  // A local unique identifier
  uint64_t id_;
  // Offset to the content
  uint64_t offset_;
  // Penalty expiration
  uint64_t penaltyExpiration_ms_;
  // Attributes key/values pairs for the flow record
  std::map<std::string, std::string> attributes_;
  // Pointer to the associated content resource claim
  FlowFileOwnedResourceClaimPtr claim_;
  // Pointers to stashed content resource claims
  std::map<std::string, FlowFileOwnedResourceClaimPtr> stashedContent_;
  // UUID string
  // std::string uuid_str_;
  // UUID string for all parents
  std::set<std::string> lineage_Identifiers_;

  // Connection queue that this flow file will be transfer or current in
  std::shared_ptr<core::Connectable> connection_;
  // Orginal connection queue that this flow file was dequeued from
  std::shared_ptr<core::Connectable> original_connection_;

 private:
  static std::shared_ptr<logging::Logger> logger_;
  static std::shared_ptr<utils::IdGenerator> id_generator_;
  static std::shared_ptr<utils::NonRepeatingStringGenerator> numeric_id_generator_;
};

}  // namespace core
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org

#endif  // LIBMINIFI_INCLUDE_CORE_FLOWFILE_H_
