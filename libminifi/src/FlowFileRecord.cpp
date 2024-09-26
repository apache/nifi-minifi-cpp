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
#include <ctime>
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

namespace org::apache::nifi::minifi {

std::shared_ptr<core::logging::Logger> FlowFileRecordImpl::logger_ = core::logging::LoggerFactory<FlowFileRecord>::getLogger();
std::atomic<uint64_t> FlowFileRecordImpl::local_flow_seq_number_(0);

FlowFileRecordImpl::FlowFileRecordImpl() {
  // TODO(adebreceni):
  //  we should revisit if we need these in a follow-up ticket
  id_ = local_flow_seq_number_++;
  addAttribute(core::SpecialFlowAttribute::FILENAME, std::to_string(utils::timeutils::getTimeNano()));
}

std::shared_ptr<FlowFileRecord> FlowFileRecordImpl::DeSerialize(const std::string& key, const std::shared_ptr<core::Repository>& flowRepository,
    const std::shared_ptr<core::ContentRepository>& content_repo, utils::Identifier& container) {
  std::string value;

  if (!flowRepository->Get(key, value)) {
    logger_->log_error("NiFi FlowFile Store event {} can not found", key);
    return nullptr;
  }
  io::BufferStream stream(value);

  auto record = DeSerialize(stream, content_repo, container);

  if (record) {
    logger_->log_debug("NiFi FlowFile retrieve uuid {} size {} connection {} success", record->getUUIDStr(), stream.size(), container.to_string());
  } else {
    logger_->log_debug("Couldn't deserialize FlowFile {} from the stream of size {}", key, stream.size());
  }

  return record;
}

bool FlowFileRecordImpl::Serialize(io::OutputStream &outStream) {
  {
    uint64_t event_time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(event_time_.time_since_epoch()).count();
    const auto ret = outStream.write(event_time_ms);
    if (ret != 8) {
      return false;
    }
  }
  {
    uint64_t entry_date_ms = std::chrono::duration_cast<std::chrono::milliseconds>(entry_date_.time_since_epoch()).count();
    const auto ret = outStream.write(entry_date_ms);
    if (ret != 8) {
      return false;
    }
  }
  {
    uint64_t lineage_start_date_ms = std::chrono::duration_cast<std::chrono::milliseconds>(lineage_start_date_.time_since_epoch()).count();
    const auto ret = outStream.write(lineage_start_date_ms);
    if (ret != 8) {
      return false;
    }
  }
  {
    const auto ret = outStream.write(uuid_);
    if (ret == 0 || io::isError(ret)) {
      return false;
    }
  }
  utils::Identifier containerId;
  if (connection_) {
    containerId = connection_->getUUID();
  }
  {
    const auto ret = outStream.write(containerId);
    if (ret == 0 || io::isError(ret)) {
      return false;
    }
  }
  // write flow attributes
  {
    const auto numAttributes = gsl::narrow<uint32_t>(attributes_.size());
    const auto ret = outStream.write(numAttributes);
    if (ret != 4) {
      return false;
    }
  }

  for (auto& itAttribute : attributes_) {
    {
      const auto ret = outStream.write(itAttribute.first, true);
      if (ret == 0 || io::isError(ret)) {
        return false;
      }
    }
    {
      const auto ret = outStream.write(itAttribute.second, true);
      if (ret == 0 || io::isError(ret)) {
        return false;
      }
    }
  }

  {
    const auto ret = outStream.write(getContentFullPath());
    if (ret == 0 || io::isError(ret)) {
      return false;
    }
  }
  {
    const auto ret = outStream.write(size_);
    if (ret != 8) {
      return false;
    }
  }
  {
    const auto ret = outStream.write(offset_);
    if (ret != 8) {
      return false;
    }
  }
  return true;
}

bool FlowFileRecordImpl::Persist(const std::shared_ptr<core::Repository>& flowRepository) {
  if (flowRepository->isNoop()) {
    return true;
  }

  io::BufferStream outStream;

  if (!Serialize(outStream)) {
    return false;
  }

  if (flowRepository->Put(getUUIDStr(), reinterpret_cast<uint8_t*>(const_cast<std::byte*>(outStream.getBuffer().data())), outStream.size())) {
    logger_->log_debug("NiFi FlowFile Store event {} size {} success", getUUIDStr(), outStream.size());
    // on behalf of the persisted record instance
    if (claim_) claim_->increaseFlowFileRecordOwnedCount();
    return true;
  } else {
    logger_->log_error("NiFi FlowFile Store failed {} size {}", getUUIDStr(), outStream.size());
    return false;
  }

  return true;
}

std::shared_ptr<FlowFileRecord> FlowFileRecordImpl::DeSerialize(io::InputStream& inStream, const std::shared_ptr<core::ContentRepository>& content_repo, utils::Identifier& container) {
  auto file = std::make_shared<FlowFileRecordImpl>();

  {
    uint64_t event_time_in_ms;
    const auto ret = inStream.read(event_time_in_ms);
    if (ret != 8) {
      return {};
    }
    file->event_time_ = std::chrono::system_clock::time_point() + std::chrono::milliseconds(event_time_in_ms);
  }

  {
    uint64_t entry_date_in_ms;
    const auto ret = inStream.read(entry_date_in_ms);
    if (ret != 8) {
      return {};
    }
    file->entry_date_ = std::chrono::system_clock::time_point() + std::chrono::milliseconds(entry_date_in_ms);
  }

  {
    uint64_t lineage_start_date_in_ms;
    const auto ret = inStream.read(lineage_start_date_in_ms);
    if (ret != 8) {
      return {};
    }
    file->lineage_start_date_ = std::chrono::system_clock::time_point() + std::chrono::milliseconds(lineage_start_date_in_ms);
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

  file->claim_ = std::make_shared<ResourceClaimImpl>(content_full_path, content_repo);

  return file;
}

std::shared_ptr<core::FlowFile> core::FlowFile::create() {
  return std::make_shared<FlowFileRecordImpl>();
}

std::shared_ptr<FlowFileRecord> FlowFileRecord::DeSerialize(std::span<const std::byte> buffer, const std::shared_ptr<core::ContentRepository> &content_repo, utils::Identifier &container) {
  return FlowFileRecordImpl::DeSerialize(buffer, content_repo, container);
}

std::shared_ptr<FlowFileRecord> FlowFileRecord::DeSerialize(io::InputStream &stream, const std::shared_ptr<core::ContentRepository> &content_repo, utils::Identifier &container) {
  return FlowFileRecordImpl::DeSerialize(stream, content_repo, container);
}

std::shared_ptr<FlowFileRecord> FlowFileRecord::DeSerialize(const std::string& key, const std::shared_ptr<core::Repository>& flowRepository,
                                                   const std::shared_ptr<core::ContentRepository> &content_repo, utils::Identifier &container) {
  return FlowFileRecordImpl::DeSerialize(key, flowRepository, content_repo, container);
}

}  // namespace org::apache::nifi::minifi
