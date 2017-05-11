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
namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace core {

FlowFile::FlowFile()
    : size_(0),
      id_(0),
      stored(false),
      offset_(0),
      last_queue_date_(0),
      penaltyExpiration_ms_(0),
      event_time_(0),
      claim_(nullptr),
      marked_delete_(false),
      connection_(nullptr),
      original_connection_(),
      logger_(logging::LoggerFactory<FlowFile>::getLogger()) {
  entry_date_ = getTimeMillis();
  lineage_start_date_ = entry_date_;

  char uuidStr[37];

  // Generate the global UUID for the flow record
  uuid_generate(uuid_);

  uuid_unparse_lower(uuid_, uuidStr);
  uuid_str_ = uuidStr;
}

FlowFile::~FlowFile() {
}

FlowFile& FlowFile::operator=(const FlowFile& other) {
  uuid_copy(uuid_, other.uuid_);
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
  uuid_str_ = other.uuid_str_;
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
void FlowFile::setOriginalConnection(
    std::shared_ptr<core::Connectable> &connection) {
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
