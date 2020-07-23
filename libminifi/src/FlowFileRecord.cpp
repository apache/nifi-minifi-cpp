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

namespace org {
namespace apache {
namespace nifi {
namespace minifi {

std::shared_ptr<logging::Logger> FlowFileRecord::logger_ = logging::LoggerFactory<FlowFileRecord>::getLogger();
std::atomic<uint64_t> FlowFileRecord::local_flow_seq_number_(0);

utils::optional<FlowFileRecord> FlowFileRecord::DeSerialize(const std::string& key, const std::shared_ptr<core::Repository>& flowRepository, const std::shared_ptr<core::ContentRepository>& content_repo) {
  std::string value;

  if (!flowRepository->Get(key, value)) {
    logger_->log_error("NiFi FlowFile Store event %s can not found", key);
    return utils::nullopt;
  }
  io::BufferStream stream((const uint8_t*) value.data(), value.length());

  auto record = DeSerialize(stream, content_repo);

  if (record) {
    logger_->log_debug("NiFi FlowFile retrieve uuid %s size " "%" PRIu64 " connection %s success", record->file_->getUUIDStr(), stream.size(), record->connectionUUID_.to_string());
  } else {
    logger_->log_debug("Couldn't deserialize FlowFile %s from the stream of size " "%" PRIu64, key, stream.size());
  }

  return record;
}

bool FlowFileRecord::Serialize(io::BufferStream &outStream) {
  int ret;

  ret = outStream.write(file->event_time_);
  if (ret != 8) {
    return false;
  }

  ret = outStream.write(file->entry_date_);
  if (ret != 8) {
    return false;
  }

  ret = outStream.write(file->lineage_start_date_);
  if (ret != 8) {
    return false;
  }

  ret = outStream.write(file->getUUIDStr());
  if (ret <= 0) {
    return false;
  }

  ret = outStream.write(connectionUUID_.to_string());
  if (ret <= 0) {
    return false;
  }
  // write flow attributes
  uint32_t numAttributes = file->attributes_.size();
  ret = outStream.write(numAttributes);
  if (ret != 4) {
    return false;
  }

  for (auto& itAttribute : file_->attributes_) {
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

  ret = outStream.write(file->size_);
  if (ret != 8) {
    return false;
  }

  ret = outStream.write(file->offset_);
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

  if (flowRepository->Put(file_->getUUIDStr(), const_cast<uint8_t*>(outStream.getBuffer()), outStream.size())) {
    logger_->log_debug("NiFi FlowFile Store event %s size " "%" PRIu64 " success", file_->getUUIDStr(), outStream.size());
    // on behalf of the persisted record instance
    if (file_->claim_) file_->claim_->increaseFlowFileRecordOwnedCount();
    return true;
  } else {
    logger_->log_error("NiFi FlowFile Store failed %s size " "%" PRIu64 " fail", file_->getUUIDStr(), outStream.size());
    return false;
  }

  return true;
}

utils::optional<FlowFileRecord> FlowFileRecord::DeSerialize(const uint8_t *buffer, const int bufferSize, const std::shared_ptr<core::ContentRepository>& content_repo) {
  int ret;

  FlowFileRecord record(std::make_shared<core::FlowFile>());
  auto& file = record.file_;

  io::DataStream outStream(buffer, bufferSize);

  ret = outStream.read(file->event_time_);
  if (ret != 8) {
    return {};
  }

  ret = outStream.read(file->entry_date_);
  if (ret != 8) {
    return {};
  }

  ret = outStream.read(file->lineage_start_date_);
  if (ret != 8) {
    return {};
  }

  std::string uuidStr;
  ret = outStream.read(uuidStr)
  if (ret <= 0) {
    return {};
  }
  utils::optional<utils::Identifier> parsedUUID = utils::Identifier::parse(uuidStr);
  if (!parsedUUID) {
    return {};
  }
  file->uuid_ = parsedUUID.value();


  std::string connectionUUIDStr;
  ret = outStream.readUTF(connectionUUIDStr);
  if (ret <= 0) {
    return {};
  }
  utils::optional<utils::Identifier> parsedConnectionUUID = utils::Identifier::parse(connectionUUIDStr);
  if (!parsedConnectionUUID) {
    return {};
  }
  record.connectionUUID_ = parsedConnectionUUID.value();

  // read flow attributes
  uint32_t numAttributes = 0;
  ret = outStream.read(numAttributes);
  if (ret != 4) {
    return {};
  }

  for (uint32_t i = 0; i < numAttributes; i++) {
    std::string key;
    ret = outStream.read(key, true);
    if (ret <= 0) {
      return {};
    }
    std::string value;
    ret = outStream.read(value, true);
    if (ret <= 0) {
      return {};
    }
    file->attributes_[key] = value;
  }

  std::string content_full_path;
  ret = outStream.readUTF(content_full_path);
  if (ret <= 0) {
    return {};
  }

  ret = outStream.read(file->size_);
  if (ret != 8) {
    return {};
  }

  ret = outStream.read(file->offset_);
  if (ret != 8) {
    return {};
  }

  claim_ = std::make_shared<ResourceClaim>(content_full_path, content_repo_);
  return record;
}

} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
