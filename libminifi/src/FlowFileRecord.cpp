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
#include <time.h>
#include <cstdio>
#include <vector>
#include <queue>
#include <map>
#include <memory>
#include <string>
#include <iostream>
#include <fstream>
#include <cinttypes>
#include "FlowFileRecord.h"
#include "core/logging/LoggerConfiguration.h"
#include "core/Relationship.h"
#include "core/Repository.h"
#include "utils/gsl.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {

std::shared_ptr<logging::Logger> FlowFileRecord::logger_ = logging::LoggerFactory<FlowFileRecord>::getLogger();
std::atomic<uint64_t> FlowFileRecord::local_flow_seq_number_(0);

FlowFileRecord::FlowFileRecord() {
  // TODO(adebreceni):
  //  we should revisit if we need these in a follow-up ticket
  id_ = local_flow_seq_number_++;
  addAttribute(core::SpecialFlowAttribute::FILENAME, std::to_string(utils::timeutils::getTimeNano()));
}

std::shared_ptr<FlowFileRecord> FlowFileRecord::DeSerialize(const std::string& key, const std::shared_ptr<core::Repository>& flowRepository,
    const std::shared_ptr<core::ContentRepository>& content_repo, utils::Identifier& container) {
  std::string value;

  if (!flowRepository->Get(key, value)) {
    logger_->log_error("NiFi FlowFile Store event %s can not found", key);
    return nullptr;
  }
  io::BufferStream stream((const uint8_t*) value.data(), gsl::narrow<int>(value.length()));

  auto record = DeSerialize(stream, content_repo, container);

  if (record) {
    logger_->log_debug("NiFi FlowFile retrieve uuid %s size " "%" PRIu64 " connection %s success", record->getUUIDStr(), stream.size(), container.to_string());
  } else {
    logger_->log_debug("Couldn't deserialize FlowFile %s from the stream of size " "%" PRIu64, key, stream.size());
  }

  return record;
}

bool FlowFileRecord::Serialize(io::OutputStream &outStream) {
  int ret;

  ret = outStream.write(event_time_);
  if (ret != 8) {
    return false;
  }

  ret = outStream.write(entry_date_);
  if (ret != 8) {
    return false;
  }

  ret = outStream.write(lineage_start_date_);
  if (ret != 8) {
    return false;
  }

  ret = outStream.write(uuid_);
  if (ret <= 0) {
    return false;
  }

  utils::Identifier containerId;
  if (connection_) {
    containerId = connection_->getUUID();
  }
  ret = outStream.write(containerId);
  if (ret <= 0) {
    return false;
  }
  // write flow attributes
  uint32_t numAttributes = gsl::narrow<uint32_t>(attributes_.size());
  ret = outStream.write(numAttributes);
  if (ret != 4) {
    return false;
  }

  for (auto& itAttribute : attributes_) {
    ret = outStream.write(itAttribute.first, true);
    if (ret <= 0) {
      return false;
    }
    ret = outStream.write(itAttribute.second, true);
    if (ret <= 0) {
      return false;
    }
  }

  ret = outStream.write(getContentFullPath());
  if (ret <= 0) {
    return false;
  }

  ret = outStream.write(size_);
  if (ret != 8) {
    return false;
  }

  ret = outStream.write(offset_);
  if (ret != 8) {
    return false;
  }

  return true;
}

bool FlowFileRecord::Persist(const std::shared_ptr<core::Repository>& flowRepository) {
  if (flowRepository->isNoop()) {
    return true;
  }

  io::BufferStream outStream;

  if (!Serialize(outStream)) {
    return false;
  }

  if (flowRepository->Put(getUUIDStr(), const_cast<uint8_t*>(outStream.getBuffer()), outStream.size())) {
    logger_->log_debug("NiFi FlowFile Store event %s size " "%" PRIu64 " success", getUUIDStr(), outStream.size());
    // on behalf of the persisted record instance
    if (claim_) claim_->increaseFlowFileRecordOwnedCount();
    return true;
  } else {
    logger_->log_error("NiFi FlowFile Store failed %s size " "%" PRIu64, getUUIDStr(), outStream.size());
    return false;
  }

  return true;
}

std::shared_ptr<FlowFileRecord> FlowFileRecord::DeSerialize(io::InputStream& inStream, const std::shared_ptr<core::ContentRepository>& content_repo, utils::Identifier& container) {
  auto file = std::make_shared<FlowFileRecord>();

  {
    const auto ret = inStream.read(file->event_time_);
    if (ret != 8) {
      return {};
    }
  }

  {
    const auto ret = inStream.read(file->entry_date_);
    if (ret != 8) {
      return {};
    }
  }

  {
    const auto ret = inStream.read(file->lineage_start_date_);
    if (ret != 8) {
      return {};
    }
  }

  {
    const auto ret = inStream.read(file->uuid_);
    if (ret == 0 || io::isError(ret)) {
      return {};
    }
  }

  {
    const auto ret = inStream.read(container);
    if (ret == 0 || io::isError(ret)) {
      return {};
    }
  }

  // read flow attributes
  uint32_t numAttributes = 0;
  {
    const auto ret = inStream.read(numAttributes);
    if (ret != 4) {
      return {};
    }
  }

  for (uint32_t i = 0; i < numAttributes; i++) {
    std::string key;
    {
      const auto ret = inStream.read(key, true);
      if (ret == 0 || io::isError(ret)) {
        return {};
      }
    }
    std::string value;
    {
      const auto ret = inStream.read(value, true);
      if (ret == 0 || io::isError(ret)) {
        return {};
      }
    }
    file->attributes_[key] = value;
  }

  std::string content_full_path;
  {
    const auto ret = inStream.read(content_full_path);
    if (ret == 0 || io::isError(ret)) {
      return {};
    }
  }

  {
    const auto ret = inStream.read(file->size_);
    if (ret != 8) {
      return {};
    }
  }

  {
    const auto ret = inStream.read(file->offset_);
    if (ret != 8) {
      return {};
    }
  }

  file->claim_ = std::make_shared<ResourceClaim>(content_full_path, content_repo);

  return file;
}

} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
