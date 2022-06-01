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

#include <memory>
#include <string>
#include <set>
#include <cinttypes>
#include "core/Repository.h"
#include "core/logging/LoggerConfiguration.h"
#include "utils/Id.h"
#include "core/FlowFile.h"
#include "utils/requirements/Container.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace core {

std::shared_ptr<utils::IdGenerator> FlowFile::id_generator_ = utils::IdGenerator::getIdGenerator();
std::shared_ptr<utils::NonRepeatingStringGenerator> FlowFile::numeric_id_generator_ = std::make_shared<utils::NonRepeatingStringGenerator>();
std::shared_ptr<logging::Logger> FlowFile::logger_ = logging::LoggerFactory<FlowFile>::getLogger();

FlowFile::FlowFile()
    : CoreComponent("FlowFile"),
      stored(false),
      marked_delete_(false),
      last_queue_date_(0),
      size_(0),
      id_(0),
      offset_(0),
      to_be_processed_after_(std::chrono::steady_clock::now()),
      claim_(nullptr) {
  id_ = numeric_id_generator_->generateId();
  entry_date_ = std::chrono::system_clock::now();
  event_time_ = entry_date_;
  lineage_start_date_ = entry_date_;
}

FlowFile& FlowFile::operator=(const FlowFile& other) {
  if (this == &other) {
    return *this;
  }
  uuid_ = other.uuid_;
  stored = other.stored;
  marked_delete_ = other.marked_delete_;
  entry_date_ = other.entry_date_;
  lineage_start_date_ = other.lineage_start_date_;
  lineage_Identifiers_ = other.lineage_Identifiers_;
  last_queue_date_ = other.last_queue_date_;
  size_ = other.size_;
  to_be_processed_after_ = other.to_be_processed_after_;
  attributes_ = other.attributes_;
  claim_ = other.claim_;
  connection_ = other.connection_;
  return *this;
}

/**
 * Returns whether or not this flow file record
 * is marked as deleted.
 * @return marked deleted
 */
bool FlowFile::isDeleted() const {
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

std::shared_ptr<ResourceClaim> FlowFile::getResourceClaim() const {
  return claim_;
}

void FlowFile::clearResourceClaim() {
  claim_ = nullptr;
}
void FlowFile::setResourceClaim(const std::shared_ptr<ResourceClaim>& claim) {
  claim_ = claim;
}

std::shared_ptr<ResourceClaim> FlowFile::getStashClaim(const std::string& key) {
  return stashedContent_[key];
}

void FlowFile::setStashClaim(const std::string& key, const std::shared_ptr<ResourceClaim>& claim) {
  if (hasStashClaim(key)) {
    logger_->log_warn("Stashing content of record %s to existing key %s; "
                      "existing content will be overwritten",
                      getUUIDStr(), key.c_str());
  }

  stashedContent_[key] = claim;
}

void FlowFile::clearStashClaim(const std::string& key) {
  auto claimIt = stashedContent_.find(key);
  if (claimIt != stashedContent_.end()) {
    claimIt->second = nullptr;
    stashedContent_.erase(claimIt);
  }
}

bool FlowFile::hasStashClaim(const std::string& key) {
  return stashedContent_.find(key) != stashedContent_.end();
}

// ! Get Entry Date
std::chrono::system_clock::time_point FlowFile::getEntryDate() const {
  return entry_date_;
}
std::chrono::system_clock::time_point FlowFile::getEventTime() const {
  return event_time_;
}
// ! Get Lineage Start Date
std::chrono::system_clock::time_point FlowFile::getlineageStartDate() const {
  return lineage_start_date_;
}

std::vector<utils::Identifier> &FlowFile::getlineageIdentifiers() {
  return lineage_Identifiers_;
}

bool FlowFile::getAttribute(const std::string& key, std::string& value) const {
  const auto attribute = getAttribute(key);
  if (!attribute) {
    return false;
  }
  value = attribute.value();
  return true;
}

std::optional<std::string> FlowFile::getAttribute(const std::string& key) const {
  auto it = attributes_.find(key);
  if (it != attributes_.end()) {
    return it->second;
  }
  return std::nullopt;
}

// Get Size
uint64_t FlowFile::getSize() const {
  return size_;
}
// ! Get Offset
uint64_t FlowFile::getOffset() const {
  return offset_;
}

bool FlowFile::removeAttribute(const std::string key) {
  auto it = attributes_.find(key);
  if (it != attributes_.end()) {
    attributes_.erase(it);
    return true;
  } else {
    return false;
  }
}

bool FlowFile::updateAttribute(const std::string key, const std::string value) {
  auto it = attributes_.find(key);
  if (it != attributes_.end()) {
    it->second = value;
    return true;
  } else {
    return false;
  }
}

bool FlowFile::addAttribute(const std::string& key, const std::string& value) {
  auto it = attributes_.find(key);
  if (it != attributes_.end()) {
    // attribute already there in the map
    return false;
  } else {
    attributes_[key] = value;
    return true;
  }
}

void FlowFile::setLineageStartDate(const std::chrono::system_clock::time_point date) {
  lineage_start_date_ = date;
}

/**
 * Sets the original connection with a shared pointer.
 * @param connection shared connection.
 */
void FlowFile::setConnection(core::Connectable* connection) {
  connection_ = connection;
}

/**
 * Returns the original connection referenced by this record.
 * @return shared original connection pointer.
 */
core::Connectable* FlowFile::getConnection() const {
  return connection_;
}

const std::string SpecialFlowAttribute::PATH = "path";
const std::string SpecialFlowAttribute::ABSOLUTE_PATH = "absolute.path";
const std::string SpecialFlowAttribute::FILENAME = "filename";
const std::string SpecialFlowAttribute::UUID = "uuid";
const std::string SpecialFlowAttribute::priority = "priority";
const std::string SpecialFlowAttribute::MIME_TYPE = "mime.type";
const std::string SpecialFlowAttribute::DISCARD_REASON = "discard.reason";
const std::string SpecialFlowAttribute::ALTERNATE_IDENTIFIER = "alternate.identifier";
const std::string SpecialFlowAttribute::FLOW_ID = "flow.id";

} /* namespace core */

namespace utils {
template struct assert_container<core::FlowFile::AttributeMap>;
} /* namespace utils */

} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
