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
#pragma once

#include <map>
#include <memory>
#include <optional>
#include <set>
#include <unordered_set>
#include <string>
#include <utility>
#include <vector>

#include "utils/TimeUtil.h"
#include "ResourceClaim.h"
#include "core/Connectable.h"
#include "WeakReference.h"
#include "utils/Export.h"
#include "minifi-cpp/core/FlowFile.h"

namespace org::apache::nifi::minifi::core {

class Connectable;

class FlowFileImpl : public CoreComponentImpl, public ReferenceContainerImpl, public virtual FlowFile {
 public:
  FlowFileImpl();
  FlowFileImpl& operator=(const FlowFileImpl& other);

  void copy(const FlowFile& other) override {
    *this = *dynamic_cast<const FlowFileImpl*>(&other);
  }

  /**
   * Returns a pointer to this flow file record's
   * claim
   */
  [[nodiscard]] std::shared_ptr<ResourceClaim> getResourceClaim() const override;
  /**
   * Sets _claim to the inbound claim argument
   */
  void setResourceClaim(const std::shared_ptr<ResourceClaim>& claim) override;

  /**
   * clear the resource claim
   */
  void clearResourceClaim() override;

  /**
   * Returns a pointer to this flow file record's
   * claim at the given stash key
   */
  std::shared_ptr<ResourceClaim> getStashClaim(const std::string& key) override;

  /**
   * Sets the given stash key to the inbound claim argument
   */
  void setStashClaim(const std::string& key, const std::shared_ptr<ResourceClaim>& claim) override;

  /**
   * Clear the resource claim at the given stash key
   */
  void clearStashClaim(const std::string& key) override;

  /**
   * Return true if the given stash claim exists
   */
  bool hasStashClaim(const std::string& key) override;

  /**
   * Get lineage identifiers
   */
  const std::vector<utils::Identifier>& getlineageIdentifiers() const override;
  std::vector<utils::Identifier>& getlineageIdentifiers() override;

  /**
   * Returns whether or not this flow file record
   * is marked as deleted.
   * @return marked deleted
   */
  [[nodiscard]] bool isDeleted() const override;

  /**
   * Sets whether to mark this flow file record
   * as deleted
   * @param deleted deleted flag
   */
  void setDeleted(bool deleted) override;

  /**
   * Get entry date for this record
   * @return entry date uint64_t
   */
  [[nodiscard]] std::chrono::system_clock::time_point getEntryDate() const override;

  /**
   * Gets the event time.
   * @return event time.
   */
  [[nodiscard]] std::chrono::system_clock::time_point getEventTime() const override;
  /**
   * Get lineage start date
   * @return lineage start date uint64_t
   */
  [[nodiscard]] std::chrono::system_clock::time_point getlineageStartDate() const override;

  /**
   * Sets the lineage start date
   * @param date new lineage start date
   */
  void setLineageStartDate(std::chrono::system_clock::time_point date) override;

  void setLineageIdentifiers(const std::vector<utils::Identifier>& lineage_Identifiers) override {
    lineage_Identifiers_ = lineage_Identifiers;
  }
  /**
   * Obtains an attribute if it exists. If it does the value is
   * copied into value
   * @param key key to look for
   * @param value value to set
   * @return result of finding key
   */
  bool getAttribute(std::string_view key, std::string& value) const override;

  [[nodiscard]] std::optional<std::string> getAttribute(std::string_view key) const override;

  /**
   * Updates the value in the attribute map that corresponds
   * to key
   * @param key attribute name
   * @param value value to set to attribute name
   * @return result of finding key
   */
  bool updateAttribute(std::string_view key, const std::string& value) override;

  /**
   * Removes the attribute
   * @param key attribute name to remove
   * @return result of finding key
   */
  bool removeAttribute(std::string_view key) override;

  /**
   * setAttribute, if attribute already there, update it, else, add it
   */
  bool setAttribute(std::string_view key, std::string value) override {
    return attributes_.insert_or_assign(std::string{key}, std::move(value)).second;
  }

  /**
   * Returns the map of attributes
   * @return attributes.
   */
  [[nodiscard]] std::map<std::string, std::string> getAttributes() const override {
    return {attributes_.begin(), attributes_.end()};
  }

  /**
   * Returns the map of attributes
   * @return attributes.
   */
  AttributeMap *getAttributesPtr() override {
    return &attributes_;
  }

  /**
   * adds an attribute if it does not exist
   *
   */
  bool addAttribute(std::string_view key, const std::string& value) override;

  /**
   * Set the size of this record.
   * @param size size of record to set.
   */
  void setSize(const uint64_t size) override {
    size_ = size;
  }
  /**
   * Returns the size of corresponding flow file
   * @return size as a uint64_t
   */
  [[nodiscard]] uint64_t getSize() const override;

  /**
   * Sets the offset
   * @param offset offset to apply to this record.
   */
  void setOffset(const uint64_t offset) override {
    offset_ = offset;
  }

  template<typename Rep, typename Period>
  void penalize(std::chrono::duration<Rep, Period> duration) {
    to_be_processed_after_ = std::chrono::steady_clock::now() + duration;
  }

  [[nodiscard]] std::chrono::steady_clock::time_point getPenaltyExpiration() const override {
    return to_be_processed_after_;
  }

  void setPenaltyExpiration(std::chrono::time_point<std::chrono::steady_clock> to_be_processed_after) override {
    to_be_processed_after_ = to_be_processed_after;
  }

  /**
   * Gets the offset within the flow file
   * @return size as a uint64_t
   */
  [[nodiscard]] uint64_t getOffset() const override;

  [[nodiscard]] bool isPenalized() const override {
    return to_be_processed_after_ > std::chrono::steady_clock::now();
  }

  [[nodiscard]] uint64_t getId() const override {
    return id_;
  }

  /**
   * Sets the original connection with a shared pointer.
   * @param connection shared connection.
   */
  void setConnection(core::Connectable* connection) override;
  /**
   * Returns the original connection referenced by this record.
   * @return shared original connection pointer.
   */
  [[nodiscard]] Connectable* getConnection() const override;

  void setStoredToRepository(bool storedInRepository) override {
    stored = storedInRepository;
  }

  [[nodiscard]] bool isStored() const override {
    return stored;
  }

 protected:
  bool stored;
  // Mark for deletion
  bool marked_delete_;
  // Date at which the flow file entered the flow
  std::chrono::system_clock::time_point entry_date_{};
  // event time
  std::chrono::system_clock::time_point event_time_{};
  // Date at which the origin of this flow file entered the flow
  std::chrono::system_clock::time_point lineage_start_date_{};
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
  std::chrono::steady_clock::time_point to_be_processed_after_;
  // Attributes key/values pairs for the flow record
  AttributeMap attributes_;
  // Pointer to the associated content resource claim
  std::shared_ptr<ResourceClaim> claim_;
  // Pointers to stashed content resource claims
  utils::FlatMap<std::string, std::shared_ptr<ResourceClaim>> stashedContent_;
  // UUID string
  // std::string uuid_str_;
  // UUID string for all parents
  std::vector<utils::Identifier> lineage_Identifiers_;

  // Orginal connection queue that this flow file was dequeued from
  core::Connectable* connection_ = nullptr;

  static std::shared_ptr<logging::Logger> logger_;
  static std::shared_ptr<utils::IdGenerator> id_generator_;
  static std::shared_ptr<utils::NonRepeatingStringGenerator> numeric_id_generator_;
};

}  // namespace org::apache::nifi::minifi::core
