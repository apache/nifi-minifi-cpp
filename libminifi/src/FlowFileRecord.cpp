/**
 * @file FlowFileRecord.cpp
 * Flow file record class implementation
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
#include "FlowFileRecord.h"
#include <time.h>
#include <cstdio>
#include <vector>
#include <queue>
#include <map>
#include <memory>
#include <string>
#include <iostream>
#include <fstream>
#include "core/logging/LoggerConfiguration.h"
#include "core/Relationship.h"
#include "core/Repository.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {

std::shared_ptr<logging::Logger> FlowFileRecord::logger_ = logging::LoggerFactory<FlowFileRecord>::getLogger();
std::atomic<uint64_t> FlowFileRecord::local_flow_seq_number_(0);

FlowFileRecord::FlowFileRecord(std::shared_ptr<core::Repository> flow_repository, const std::shared_ptr<core::ContentRepository> &content_repo, std::map<std::string, std::string> attributes,
                               std::shared_ptr<ResourceClaim> claim)
    : FlowFile(),
      content_repo_(content_repo),
      flow_repository_(flow_repository) {
  id_ = local_flow_seq_number_.load();
  claim_ = claim;
  // Increase the local ID for the flow record
  ++local_flow_seq_number_;
  // Populate the default attributes
  addKeyedAttribute(FILENAME, std::to_string(getTimeNano()));
  addKeyedAttribute(PATH, DEFAULT_FLOWFILE_PATH);
  addKeyedAttribute(UUID, getUUIDStr());
  // Populate the attributes from the input
  std::map<std::string, std::string>::iterator it;
  for (it = attributes.begin(); it != attributes.end(); it++) {
    FlowFile::addAttribute(it->first, it->second);
  }

  snapshot_ = false;

  if (claim_ != nullptr) {
    // Increase the flow file record owned count for the resource claim
    claim_->increaseFlowFileRecordOwnedCount();
    content_full_fath_ = claim->getContentFullPath();
  }
}

FlowFileRecord::FlowFileRecord(std::shared_ptr<core::Repository> flow_repository, const std::shared_ptr<core::ContentRepository> &content_repo, std::shared_ptr<core::FlowFile> &event,
                               const std::string &uuidConnection)
    : FlowFile(),
      snapshot_(""),
      content_repo_(content_repo),
      flow_repository_(flow_repository) {
  entry_date_ = event->getEntryDate();
  lineage_start_date_ = event->getlineageStartDate();
  lineage_Identifiers_ = event->getlineageIdentifiers();
  uuidStr_ = event->getUUIDStr();
  attributes_ = event->getAttributes();
  size_ = event->getSize();
  offset_ = event->getOffset();
  event->getUUID(uuid_);
  uuid_connection_ = uuidConnection;
  if (event->getResourceClaim()) {
    event->getResourceClaim()->increaseFlowFileRecordOwnedCount();
    content_full_fath_ = event->getResourceClaim()->getContentFullPath();
  }
  if (event->getFlowIdentifier()) {
    std::string attr;
    event->getAttribute(FlowAttributeKey(FlowAttribute::FLOW_ID), attr);
    setFlowIdentifier(event->getFlowIdentifier());
    if (!attr.empty()) {
      addKeyedAttribute(FlowAttribute::FLOW_ID, attr);
    }
  }
}

FlowFileRecord::FlowFileRecord(std::shared_ptr<core::Repository> flow_repository, const std::shared_ptr<core::ContentRepository> &content_repo, std::shared_ptr<core::FlowFile> &event)
    : FlowFile(),
      uuid_connection_(""),
      snapshot_(""),
      content_repo_(content_repo),
      flow_repository_(flow_repository) {
  if (event->getFlowIdentifier()) {
    std::string attr;
    event->getAttribute(FlowAttributeKey(FlowAttribute::FLOW_ID), attr);
    setFlowIdentifier(event->getFlowIdentifier());
    if (!attr.empty()) {
      addKeyedAttribute(FlowAttribute::FLOW_ID, attr);
    }
  }
}

FlowFileRecord::~FlowFileRecord() {
  logger_->log_debug("Destroying flow file record,  UUID %s", uuidStr_);
  if (!snapshot_)
    logger_->log_debug("Delete FlowFile UUID %s", uuidStr_);
  else
    logger_->log_debug("Delete SnapShot FlowFile UUID %s", uuidStr_);
  if (claim_) {
    releaseClaim(claim_);
  } else {
    logger_->log_debug("Claim is null ptr for %s", uuidStr_);
  }

  // Disown stash claims
  for (const auto &stashPair : stashedContent_) {
    releaseClaim(stashPair.second);
  }
}

void FlowFileRecord::releaseClaim(std::shared_ptr<ResourceClaim> claim) {
  // Decrease the flow file record owned count for the resource claim
  claim_->decreaseFlowFileRecordOwnedCount();
  std::string value;
  logger_->log_debug("Delete Resource Claim %s, %s, attempt %llu", getUUIDStr(), claim_->getContentFullPath(), claim_->getFlowFileRecordOwnedCount());
  if (claim_->getFlowFileRecordOwnedCount() <= 0) {
    // we cannot rely on the stored variable here since we aren't guaranteed atomicity
    if (flow_repository_ != nullptr && !flow_repository_->Get(uuidStr_, value)) {
      logger_->log_debug("Delete Resource Claim %s", claim_->getContentFullPath());
      content_repo_->remove(claim_);
    }
  }
}

bool FlowFileRecord::addKeyedAttribute(FlowAttribute key, const std::string &value) {
  const char *keyStr = FlowAttributeKey(key);
  if (keyStr) {
    const std::string keyString = keyStr;
    return FlowFile::addAttribute(keyString, value);
  } else {
    return false;
  }
}

bool FlowFileRecord::removeKeyedAttribute(FlowAttribute key) {
  const char *keyStr = FlowAttributeKey(key);
  if (keyStr) {
    std::string keyString = keyStr;
    return FlowFile::removeAttribute(keyString);
  } else {
    return false;
  }
}

bool FlowFileRecord::updateKeyedAttribute(FlowAttribute key, std::string value) {
  const char *keyStr = FlowAttributeKey(key);
  if (keyStr) {
    std::string keyString = keyStr;
    return FlowFile::updateAttribute(keyString, value);
  } else {
    return false;
  }
}

bool FlowFileRecord::getKeyedAttribute(FlowAttribute key, std::string &value) {
  const char *keyStr = FlowAttributeKey(key);
  if (keyStr) {
    std::string keyString = keyStr;
    return FlowFile::getAttribute(keyString, value);
  } else {
    return false;
  }
}

FlowFileRecord &FlowFileRecord::operator=(const FlowFileRecord &other) {
  core::FlowFile::operator=(other);
  uuid_connection_ = other.uuid_connection_;
  content_full_fath_ = other.content_full_fath_;
  snapshot_ = other.snapshot_;
  return *this;
}

bool FlowFileRecord::DeSerialize(std::string key) {
  std::string value;
  bool ret;

  ret = flow_repository_->Get(key, value);

  if (!ret) {
    logger_->log_error("NiFi FlowFile Store event %s can not found", key);
    return false;
  }
  io::DataStream stream((const uint8_t*) value.data(), value.length());

  ret = DeSerialize(stream);

  if (ret) {
    logger_->log_debug("NiFi FlowFile retrieve uuid %s size %llu connection %s success", uuidStr_, stream.getSize(), uuid_connection_);
  } else {
    logger_->log_debug("NiFi FlowFile retrieve uuid %s size %llu connection %s fail", uuidStr_, stream.getSize(), uuid_connection_);
  }

  return ret;
}

bool FlowFileRecord::Serialize() {
  io::DataStream outStream;

  int ret;

  ret = write(this->event_time_, &outStream);
  if (ret != 8) {
    return false;
  }

  ret = write(this->entry_date_, &outStream);
  if (ret != 8) {
    return false;
  }

  ret = write(this->lineage_start_date_, &outStream);
  if (ret != 8) {
    return false;
  }

  ret = writeUTF(this->uuidStr_, &outStream);
  if (ret <= 0) {
    return false;
  }

  ret = writeUTF(this->uuid_connection_, &outStream);
  if (ret <= 0) {
    return false;
  }
  // write flow attributes
  uint32_t numAttributes = this->attributes_.size();
  ret = write(numAttributes, &outStream);
  if (ret != 4) {
    return false;
  }

  for (auto itAttribute : attributes_) {
    ret = writeUTF(itAttribute.first, &outStream, true);
    if (ret <= 0) {
      return false;
    }
    ret = writeUTF(itAttribute.second, &outStream, true);
    if (ret <= 0) {
      return false;
    }
  }

  ret = writeUTF(this->content_full_fath_, &outStream);
  if (ret <= 0) {
    return false;
  }

  ret = write(this->size_, &outStream);
  if (ret != 8) {
    return false;
  }

  ret = write(this->offset_, &outStream);
  if (ret != 8) {
    return false;
  }

  if (flow_repository_->Put(uuidStr_, const_cast<uint8_t*>(outStream.getBuffer()), outStream.getSize())) {
    logger_->log_debug("NiFi FlowFile Store event %s size %llu success", uuidStr_, outStream.getSize());
    return true;
  } else {
    logger_->log_error("NiFi FlowFile Store event %s size %llu fail", uuidStr_, outStream.getSize());
    return false;
  }

  return true;
}

bool FlowFileRecord::DeSerialize(const uint8_t *buffer, const int bufferSize) {
  int ret;

  io::DataStream outStream(buffer, bufferSize);

  ret = read(this->event_time_, &outStream);
  if (ret != 8) {
    return false;
  }

  ret = read(this->entry_date_, &outStream);
  if (ret != 8) {
    return false;
  }

  ret = read(this->lineage_start_date_, &outStream);
  if (ret != 8) {
    return false;
  }

  ret = readUTF(this->uuidStr_, &outStream);
  if (ret <= 0) {
    return false;
  }

  ret = readUTF(this->uuid_connection_, &outStream);
  if (ret <= 0) {
    return false;
  }

  // read flow attributes
  uint32_t numAttributes = 0;
  ret = read(numAttributes, &outStream);
  if (ret != 4) {
    return false;
  }

  for (uint32_t i = 0; i < numAttributes; i++) {
    std::string key;
    ret = readUTF(key, &outStream, true);
    if (ret <= 0) {
      return false;
    }
    std::string value;
    ret = readUTF(value, &outStream, true);
    if (ret <= 0) {
      return false;
    }
    this->attributes_[key] = value;
  }

  ret = readUTF(this->content_full_fath_, &outStream);
  if (ret <= 0) {
    return false;
  }

  ret = read(this->size_, &outStream);
  if (ret != 8) {
    return false;
  }

  ret = read(this->offset_, &outStream);
  if (ret != 8) {
    return false;
  }

  if (nullptr == claim_) {
    claim_ = std::make_shared<ResourceClaim>(content_full_fath_, content_repo_, true);
  }
  return true;
}

} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
