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

#include "core/FlowFile.h"
#include <memory>
#include <string>
#include <set>
#include "core/logging/LoggerConfiguration.h"
#include "utils/Id.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace core {

std::shared_ptr<utils::IdGenerator> FlowFile::id_generator_ = utils::IdGenerator::getIdGenerator();
std::shared_ptr<utils::NonRepeatingStringGenerator> FlowFile::numeric_id_generator_ = std::make_shared<utils::NonRepeatingStringGenerator>();
std::shared_ptr<logging::Logger> FlowFile::logger_ = logging::LoggerFactory<FlowFile>::getLogger();

FlowFile::FlowFile()
    : Connectable("FlowFile"),
      size_(0),
      stored(false),
      offset_(0),
      last_queue_date_(0),
      penaltyExpiration_ms_(0),
      event_time_(0),
      claim_(nullptr),
      marked_delete_(false),
      connection_(nullptr),
      original_connection_() {
  id_ = numeric_id_generator_->generateId();
  entry_date_ = getTimeMillis();
  event_time_ = entry_date_;
  lineage_start_date_ = entry_date_;
}

FlowFile::~FlowFile() {
}

FlowFile& FlowFile::operator=(const FlowFile& other) {
  uuid_ = other.uuid_;
  stored = other.stored;
  marked_delete_ = other.marked_delete_;
  entry_date_ = other.entry_date_;
  lineage_start_date_ = other.lineage_start_date_;
  lineage_Identifiers_ = other.lineage_Identifiers_;
  last_queue_date_ = other.last_queue_date_;
  size_ = other.size_;
  penaltyExpiration_ms_ = other.penaltyExpiration_ms_;
  attributes_ = other.attributes_;
  claim_ = other.claim_;
  if (claim_ != nullptr)
    this->claim_->increaseFlowFileRecordOwnedCount();
  uuidStr_ = other.uuidStr_;
  connection_ = other.connection_;
  original_connection_ = other.original_connection_;
  return *this;
}

/**
 * Returns whether or not this flow file record
 * is marked as deleted.
 * @return marked deleted
 */
bool FlowFile::isDeleted() {
  return marked_delete_;
}

/**
 * Sets whether to mark this flow file record
 * as deleted
 * @param deleted deleted flag
 */
void FlowFile::setDeleted(const bool deleted) {
  marked_delete_ = deleted;
  if (marked_delete_) {
    removeReferences();
  }
}

std::shared_ptr<ResourceClaim> FlowFile::getResourceClaim() {
  return claim_;
}

void FlowFile::clearResourceClaim() {
  claim_ = nullptr;
}
void FlowFile::setResourceClaim(std::shared_ptr<ResourceClaim> &claim) {
  claim_ = claim;
}

std::shared_ptr<ResourceClaim> FlowFile::getStashClaim(const std::string &key) {
  return stashedContent_[key];
}

void FlowFile::setStashClaim(const std::string &key, const std::shared_ptr<ResourceClaim> &claim) {
  if (hasStashClaim(key)) {
    logger_->log_warn("Stashing content of record %s to existing key %s; "
                      "existing content will be overwritten",
                      getUUIDStr().c_str(), key.c_str());
    releaseClaim(getStashClaim(key));
  }

  stashedContent_[key] = claim;
}

void FlowFile::clearStashClaim(const std::string &key) {
  stashedContent_.erase(key);
}

bool FlowFile::hasStashClaim(const std::string &key) {
  return stashedContent_.find(key) != stashedContent_.end();
}

// ! Get Entry Date
uint64_t FlowFile::getEntryDate() {
  return entry_date_;
}
uint64_t FlowFile::getEventTime() {
  return event_time_;
}
// ! Get Lineage Start Date
uint64_t FlowFile::getlineageStartDate() {
  return lineage_start_date_;
}

std::set<std::string> &FlowFile::getlineageIdentifiers() {
  return lineage_Identifiers_;
}

bool FlowFile::getAttribute(std::string key, std::string &value) {
  auto it = attributes_.find(key);
  if (it != attributes_.end()) {
    value = it->second;
    return true;
  } else {
    return false;
  }
}

// Get Size
uint64_t FlowFile::getSize() {
  return size_;
}
// ! Get Offset
uint64_t FlowFile::getOffset() {
  return offset_;
}

bool FlowFile::removeAttribute(const std::string key) {
  auto it = attributes_.find(key);
  if (it != attributes_.end()) {
    attributes_.erase(key);
    return true;
  } else {
    return false;
  }
}

bool FlowFile::updateAttribute(const std::string key, const std::string value) {
  auto it = attributes_.find(key);
  if (it != attributes_.end()) {
    attributes_[key] = value;
    return true;
  } else {
    return false;
  }
}

bool FlowFile::addAttribute(const std::string &key, const std::string &value) {
  auto it = attributes_.find(key);
  if (it != attributes_.end()) {
    // attribute already there in the map
    return false;
  } else {
    attributes_[key] = value;
    return true;
  }
}

void FlowFile::setLineageStartDate(const uint64_t date) {
  lineage_start_date_ = date;
}

/**
 * Sets the original connection with a shared pointer.
 * @param connection shared connection.
 */
void FlowFile::setOriginalConnection(std::shared_ptr<core::Connectable> &connection) {
  original_connection_ = connection;
}

/**
 * Sets the connection with a shared pointer.
 * @param connection shared connection.
 */
void FlowFile::setConnection(std::shared_ptr<core::Connectable> &connection) {
  connection_ = connection;
}

/**
 * Sets the connection with a shared pointer.
 * @param connection shared connection.
 */
void FlowFile::setConnection(std::shared_ptr<core::Connectable> &&connection) {
  connection_ = connection;
}

/**
 * Returns the connection referenced by this record.
 * @return shared connection pointer.
 */
std::shared_ptr<core::Connectable> FlowFile::getConnection() {
  return connection_;
}

/**
 * Returns the original connection referenced by this record.
 * @return shared original connection pointer.
 */
std::shared_ptr<core::Connectable> FlowFile::getOriginalConnection() {
  return original_connection_;
}

} /* namespace core */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
