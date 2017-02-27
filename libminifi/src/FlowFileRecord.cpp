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
#include <vector>
#include <queue>
#include <map>
#include <sys/time.h>
#include <time.h>
#include <iostream>
#include <fstream>
#include <cstdio>

#include "FlowFileRecord.h"
#include "core/logging/Logger.h"
#include "core/Relationship.h"
#include "core/Repository.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {

std::atomic<uint64_t> FlowFileRecord::local_flow_seq_number_(0);

FlowFileRecord::FlowFileRecord(
    std::shared_ptr<core::Repository> flow_repository,
    std::map<std::string, std::string> attributes,
    std::shared_ptr<ResourceClaim> claim)
    : FlowFile(),
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

  if (claim_ != nullptr)
    // Increase the flow file record owned count for the resource claim
    claim_->increaseFlowFileRecordOwnedCount();
  logger_ = logging::Logger::getLogger();
}

FlowFileRecord::FlowFileRecord(
    std::shared_ptr<core::Repository> flow_repository,
    std::shared_ptr<core::FlowFile> event)
    : FlowFile(),
      flow_repository_(flow_repository) {

}

FlowFileRecord::~FlowFileRecord() {
  if (!snapshot_)
    logger_->log_debug("Delete FlowFile UUID %s", uuid_str_.c_str());
  else
    logger_->log_debug("Delete SnapShot FlowFile UUID %s", uuid_str_.c_str());
  if (claim_) {
    // Decrease the flow file record owned count for the resource claim
    claim_->decreaseFlowFileRecordOwnedCount();
    if (claim_->getFlowFileRecordOwnedCount() <= 0) {
      logger_->log_debug("Delete Resource Claim %s",
                         claim_->getContentFullPath().c_str());
      if (!this->stored || !flow_repository_->Get(_uuidStr, value)) {
        std::remove(claim_->getContentFullPath().c_str());
      }
    }
  }
}

bool FlowFileRecord::addKeyedAttribute(FlowAttribute key, std::string value) {
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

bool FlowFileRecord::updateKeyedAttribute(FlowAttribute key,
                                          std::string value) {
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
  return *this;
}

} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
