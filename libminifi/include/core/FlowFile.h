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
#include <optional>
#include <set>
#include <unordered_set>
#include <string>
#include <utility>
#include <vector>

#include "utils/TimeUtil.h"
#include "ResourceClaim.h"
#include "Connectable.h"
#include "WeakReference.h"
#include "utils/FlatMap.h"
#include "utils/Export.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace core {

class Connectable;

class FlowFile : public CoreComponent, public ReferenceContainer {
 public:
  FlowFile();
  FlowFile& operator=(const FlowFile& other);

  using AttributeMap = utils::FlatMap<std::string, std::string>;

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
   * Get lineage identifiers
   */
  std::vector<utils::Identifier> &getlineageIdentifiers();

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

  void setLineageIdentifiers(const std::vector<utils::Identifier>& lineage_Identifiers) {
    lineage_Identifiers_ = lineage_Identifiers;
  }
  /**
   * Obtains an attribute if it exists. If it does the value is
   * copied into value
   * @param key key to look for
   * @param value value to set
   * @return result of finding key
   */
  bool getAttribute(const std::string& key, std::string& value) const;

  std::optional<std::string> getAttribute(const std::string& key) const;

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
  bool setAttribute(const std::string& key, std::string value) {
    return attributes_.insert_or_assign(key, std::move(value)).second;
  }
  bool setAttribute(std::string&& key, std::string value) {
    return attributes_.insert_or_assign(std::move(key), std::move(value)).second;
  }

  /**
   * Returns the map of attributes
   * @return attributes.
   */
  std::map<std::string, std::string> getAttributes() const {
    return {attributes_.begin(), attributes_.end()};
  }

  /**
   * Returns the map of attributes
   * @return attributes.
   */
  AttributeMap *getAttributesPtr() {
    return &attributes_;
  }

  /**
   * adds an attribute if it does not exist
   *
   */
  bool addAttribute(const std::string& key, const std::string& value);

  /**
   * Set the size of this record.
   * @param size size of record to set.
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

  template<typename Rep, typename Period>
  void penalize(std::chrono::duration<Rep, Period> duration) {
    to_be_processed_after_ = std::chrono::steady_clock::now() + duration;
  }

  std::chrono::time_point<std::chrono::steady_clock> getPenaltyExpiration() const {
    return to_be_processed_after_;
  }

  /**
   * Gets the offset within the flow file
   * @return size as a uint64_t
   */
  uint64_t getOffset() const;

  bool isPenalized() const {
    return to_be_processed_after_ > std::chrono::steady_clock::now();
  }

  uint64_t getId() const {
    return id_;
  }

  /**
   * Sets the original connection with a shared pointer.
   * @param connection shared connection.
   */
  void setConnection(const std::shared_ptr<core::Connectable>& connection);
  /**
   * Returns the original connection referenced by this record.
   * @return shared original connection pointer.
   */
  std::shared_ptr<core::Connectable> getConnection() const;

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
  std::chrono::time_point<std::chrono::steady_clock> to_be_processed_after_;
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
  std::shared_ptr<core::Connectable> connection_;

  static std::shared_ptr<logging::Logger> logger_;
  static std::shared_ptr<utils::IdGenerator> id_generator_;
  static std::shared_ptr<utils::NonRepeatingStringGenerator> numeric_id_generator_;
};

// FlowFile Attribute
struct SpecialFlowAttribute {
  // The flowfile's path indicates the relative directory to which a FlowFile belongs and does not contain the filename
  MINIFIAPI static const std::string PATH;
  // The flowfile's absolute path indicates the absolute directory to which a FlowFile belongs and does not contain the filename
  MINIFIAPI static const std::string ABSOLUTE_PATH;
  // The filename of the FlowFile. The filename should not contain any directory structure.
  MINIFIAPI static const std::string FILENAME;
  // A unique UUID assigned to this FlowFile.
  MINIFIAPI static const std::string UUID;
  // A numeric value indicating the FlowFile priority
  MINIFIAPI static const std::string priority;
  // The MIME Type of this FlowFile
  MINIFIAPI static const std::string MIME_TYPE;
  // Specifies the reason that a FlowFile is being discarded
  MINIFIAPI static const std::string DISCARD_REASON;
  // Indicates an identifier other than the FlowFile's UUID that is known to refer to this FlowFile.
  MINIFIAPI static const std::string ALTERNATE_IDENTIFIER;
  // Flow identifier
  MINIFIAPI static const std::string FLOW_ID;

  static const auto& getSpecialFlowAttributes() {
    static const std::array<std::string_view, 9> SPECIAL_FLOW_ATTRIBUTES {
      PATH, ABSOLUTE_PATH, FILENAME, UUID, priority, MIME_TYPE, DISCARD_REASON, ALTERNATE_IDENTIFIER, FLOW_ID
    };
    return SPECIAL_FLOW_ATTRIBUTES;
  }
};

}  // namespace core
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org

#endif  // LIBMINIFI_INCLUDE_CORE_FLOWFILE_H_
