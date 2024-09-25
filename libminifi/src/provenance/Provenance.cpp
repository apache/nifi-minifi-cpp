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

#include "provenance/Provenance.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>
#include <list>
#include <utility>

#include "core/Repository.h"
#include "io/BufferStream.h"
#include "core/logging/Logger.h"
#include "core/Relationship.h"
#include "FlowController.h"
#include "utils/gsl.h"

namespace org::apache::nifi::minifi::provenance {

std::shared_ptr<utils::IdGenerator> ProvenanceEventRecordImpl::id_generator_ = utils::IdGenerator::getIdGenerator();
std::shared_ptr<core::logging::Logger> ProvenanceEventRecordImpl::logger_ = core::logging::LoggerFactory<ProvenanceEventRecord>::getLogger();

const char *ProvenanceEventRecord::ProvenanceEventTypeStr[REPLAY + 1] = { "CREATE", "RECEIVE", "FETCH", "SEND", "DOWNLOAD",  // NOLINT(cppcoreguidelines-avoid-c-arrays)
    "DROP", "EXPIRE", "FORK", "JOIN", "CLONE", "CONTENT_MODIFIED", "ATTRIBUTES_MODIFIED", "ROUTE", "ADDINFO", "REPLAY" };

ProvenanceEventRecordImpl::ProvenanceEventRecordImpl(ProvenanceEventRecord::ProvenanceEventType event, std::string componentId, std::string componentType)
    : core::SerializableComponentImpl(core::className<ProvenanceEventRecord>()),
      _eventType(event),
      _eventTime(std::chrono::system_clock::now()),
      _componentId(std::move(componentId)),
      _componentType(std::move(componentType)) {
}

bool ProvenanceEventRecordImpl::loadFromRepository(const std::shared_ptr<core::Repository> &repo) {
  std::string value;
  bool ret = false;

  if (nullptr == repo || uuid_.isNil()) {
    logger_->log_error("Repo could not be assigned");
    return false;
  }
  ret = repo->Get(getUUIDStr(), value);

  if (!ret) {
    logger_->log_error("NiFi Provenance Store event {} can not be found", getUUIDStr());
    return false;
  } else {
    logger_->log_debug("NiFi Provenance Read event {}", getUUIDStr());
  }

  org::apache::nifi::minifi::io::BufferStream stream(value);

  ret = deserialize(stream);

  if (ret) {
    logger_->log_debug("NiFi Provenance retrieve event {} size {} eventType {} success", getUUIDStr(), stream.size(), magic_enum::enum_name(_eventType));
  } else {
    logger_->log_debug("NiFi Provenance retrieve event {} size {} eventType {} fail", getUUIDStr(), stream.size(), magic_enum::enum_name(_eventType));
  }

  return ret;
}

bool ProvenanceEventRecordImpl::serialize(io::OutputStream& output_stream) {
  {
    const auto ret = output_stream.write(this->uuid_);
    if (ret == 0 || io::isError(ret)) {
      return false;
    }
  }
  {
    uint32_t eventType = this->_eventType;
    const auto ret = output_stream.write(eventType);
    if (ret != 4) {
      return false;
    }
  }
  {
    uint64_t event_time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(_eventTime.time_since_epoch()).count();
    const auto ret = output_stream.write(event_time_ms);
    if (ret != 8) {
      return false;
    }
  }
  {
    uint64_t entry_date_ms = std::chrono::duration_cast<std::chrono::milliseconds>(_entryDate.time_since_epoch()).count();
    const auto ret = output_stream.write(entry_date_ms);
    if (ret != 8) {
      return false;
    }
  }
  {
    uint64_t event_duration_ms = this->_eventDuration.count();
    const auto ret = output_stream.write(event_duration_ms);
    if (ret != 8) {
      return false;
    }
  }
  {
    uint64_t lineage_start_date_ms = std::chrono::duration_cast<std::chrono::milliseconds>(_lineageStartDate.time_since_epoch()).count();
    const auto ret = output_stream.write(lineage_start_date_ms);
    if (ret != 8) {
      return false;
    }
  }
  {
    const auto ret = output_stream.write(this->_componentId);
    if (ret == 0 || io::isError(ret)) {
      return false;
    }
  }
  {
    const auto ret = output_stream.write(this->_componentType);
    if (ret == 0 || io::isError(ret)) {
      return false;
    }
  }
  {
    const auto ret = output_stream.write(this->flow_uuid_);
    if (ret == 0 || io::isError(ret)) {
      return false;
    }
  }
  {
    const auto ret = output_stream.write(this->_details);
    if (ret == 0 || io::isError(ret)) {
      return false;
    }
  }
  // write flow attributes
  {
    const auto numAttributes = gsl::narrow<uint32_t>(this->_attributes.size());
    const auto ret = output_stream.write(numAttributes);
    if (ret != 4) {
      return false;
    }
  }
  for (const auto& itAttribute : _attributes) {
    {
      const auto ret = output_stream.write(itAttribute.first);
      if (ret == 0 || io::isError(ret)) {
        return false;
      }
    }
    {
      const auto ret = output_stream.write(itAttribute.second);
      if (ret == 0 || io::isError(ret)) {
        return false;
      }
    }
  }
  {
    const auto ret = output_stream.write(this->_contentFullPath);
    if (ret == 0 || io::isError(ret)) {
      return false;
    }
  }
  {
    const auto ret = output_stream.write(this->_size);
    if (ret != 8) {
      return false;
    }
  }
  {
    const auto ret = output_stream.write(this->_offset);
    if (ret != 8) {
      return false;
    }
  }
  {
    const auto ret = output_stream.write(this->_sourceQueueIdentifier);
    if (ret == 0 || io::isError(ret)) {
      return false;
    }
  }
  if (this->_eventType == ProvenanceEventRecord::FORK || this->_eventType == ProvenanceEventRecord::CLONE || this->_eventType == ProvenanceEventRecord::JOIN) {
    // write UUIDs
    {
      const auto parent_uuids_count = gsl::narrow<uint32_t>(this->_parentUuids.size());
      const auto ret = output_stream.write(parent_uuids_count);
      if (ret != 4) {
        return false;
      }
    }
    for (const auto& parentUUID : _parentUuids) {
      const auto ret = output_stream.write(parentUUID);
      if (ret == 0 || io::isError(ret)) {
        return false;
      }
    }
    {
      const auto children_uuids_count = gsl::narrow<uint32_t>(this->_childrenUuids.size());
      const auto ret = output_stream.write(children_uuids_count);
      if (ret != 4) {
        return false;
      }
    }
    for (const auto& childUUID : _childrenUuids) {
      const auto ret = output_stream.write(childUUID);
      if (ret == 0 || io::isError(ret)) {
        return false;
      }
    }
  } else if (this->_eventType == ProvenanceEventRecord::SEND || this->_eventType == ProvenanceEventRecord::FETCH) {
    const auto ret = output_stream.write(this->_transitUri);
    if (ret == 0 || io::isError(ret)) {
      return false;
    }
  } else if (this->_eventType == ProvenanceEventRecord::RECEIVE) {
    {
      const auto ret = output_stream.write(this->_transitUri);
      if (ret == 0 || io::isError(ret)) {
        return false;
      }
    }
    {
      const auto ret = output_stream.write(this->_sourceSystemFlowFileIdentifier);
      if (ret == 0 || io::isError(ret)) {
        return false;
      }
    }
  }

  return true;
}

bool ProvenanceEventRecordImpl::deserialize(io::InputStream &input_stream) {
  {
    const auto ret = input_stream.read(uuid_);
    if (ret == 0 || io::isError(ret)) {
      return false;
    }
  }

  uint32_t eventType = 0;
  {
    const auto ret = input_stream.read(eventType);
    if (ret != 4) {
      return false;
    }
  }

  if (auto event_type_opt = magic_enum::enum_cast<ProvenanceEventRecord::ProvenanceEventType>(eventType)) {
    _eventType = *event_type_opt;
  } else {
    return false;
  }

  {
    uint64_t event_time_in_ms = 0;
    const auto ret = input_stream.read(event_time_in_ms);
    if (ret != 8) {
      return false;
    }
    _eventTime = std::chrono::system_clock::time_point() + std::chrono::milliseconds(event_time_in_ms);
  }

  {
    uint64_t entry_date_in_ms = 0;
    const auto ret = input_stream.read(entry_date_in_ms);
    if (ret != 8) {
      return false;
    }
    _entryDate = std::chrono::system_clock::time_point() + std::chrono::milliseconds(entry_date_in_ms);
  }

  {
    uint64_t event_duration_ms = 0;
    const auto ret = input_stream.read(event_duration_ms);
    if (ret != 8) {
      return false;
    }
    _eventDuration = std::chrono::milliseconds(event_duration_ms);
  }

  {
    uint64_t lineage_start_date_in_ms = 0;
    const auto ret = input_stream.read(lineage_start_date_in_ms);
    if (ret != 8) {
      return false;
    }
    _lineageStartDate = std::chrono::system_clock::time_point() + std::chrono::milliseconds(lineage_start_date_in_ms);
  }

  {
    const auto ret = input_stream.read(this->_componentId);
    if (ret == 0 || io::isError(ret)) {
      return false;
    }
  }

  {
    const auto ret = input_stream.read(this->_componentType);
    if (ret == 0 || io::isError(ret)) {
      return false;
    }
  }

  {
    const auto ret = input_stream.read(this->flow_uuid_);
    if (ret == 0 || io::isError(ret)) {
      return false;
    }
  }

  {
    const auto ret = input_stream.read(this->_details);
    if (ret == 0 || io::isError(ret)) {
      return false;
    }
  }

  // read flow attributes
  uint32_t numAttributes = 0;
  {
    const auto ret = input_stream.read(numAttributes);
    if (ret != 4) {
      return false;
    }
  }

  for (uint32_t i = 0; i < numAttributes; i++) {
    std::string key;
    {
      const auto ret = input_stream.read(key);
      if (ret == 0 || io::isError(ret)) {
        return false;
      }
    }
    std::string value;
    {
      const auto ret = input_stream.read(value);
      if (ret == 0 || io::isError(ret)) {
        return false;
      }
    }
    this->_attributes[key] = value;
  }

  {
    const auto ret = input_stream.read(this->_contentFullPath);
    if (ret == 0 || io::isError(ret)) {
      return false;
    }
  }

  {
    const auto ret = input_stream.read(this->_size);
    if (ret != 8) {
      return false;
    }
  }

  {
    const auto ret = input_stream.read(this->_offset);
    if (ret != 8) {
      return false;
    }
  }

  {
    const auto ret = input_stream.read(this->_sourceQueueIdentifier);
    if (ret == 0 || io::isError(ret)) {
      return false;
    }
  }

  if (this->_eventType == ProvenanceEventRecord::FORK || this->_eventType == ProvenanceEventRecord::CLONE || this->_eventType == ProvenanceEventRecord::JOIN) {
    // read UUIDs
    uint32_t number = 0;
    {
      const auto ret = input_stream.read(number);
      if (ret != 4) {
        return false;
      }
    }

    for (uint32_t i = 0; i < number; i++) {
      utils::Identifier parentUUID;
      {
        const auto ret = input_stream.read(parentUUID);
        if (ret == 0 || io::isError(ret)) {
          return false;
        }
      }
      this->addParentUuid(parentUUID);
    }
    number = 0;
    {
      const auto ret = input_stream.read(number);
      if (ret != 4) {
        return false;
      }
    }
    for (uint32_t i = 0; i < number; i++) {
      utils::Identifier childUUID;
      {
        const auto ret = input_stream.read(childUUID);
        if (ret == 0 || io::isError(ret)) {
          return false;
        }
      }
      this->addChildUuid(childUUID);
    }
  } else if (this->_eventType == ProvenanceEventRecord::SEND || this->_eventType == ProvenanceEventRecord::FETCH) {
    {
      const auto ret = input_stream.read(this->_transitUri);
      if (ret == 0 || io::isError(ret)) {
        return false;
      }
    }
  } else if (this->_eventType == ProvenanceEventRecord::RECEIVE) {
    {
      const auto ret = input_stream.read(this->_transitUri);
      if (ret == 0 || io::isError(ret)) {
        return false;
      }
    }
    {
      const auto ret = input_stream.read(this->_sourceSystemFlowFileIdentifier);
      if (ret == 0 || io::isError(ret)) {
        return false;
      }
    }
  }

  return true;
}

void ProvenanceReporterImpl::commit() {
  if (repo_->isNoop()) {
    return;
  }

  if (repo_->isFull()) {
    logger_->log_debug("Provenance Repository is full");
    return;
  }

  std::vector<std::pair<std::string, std::unique_ptr<io::BufferStream>>> flowData;

  for (auto& event : _events) {
    std::unique_ptr<io::BufferStream> stramptr(new io::BufferStream());
    event->serialize(*stramptr);

    flowData.emplace_back(event->getUUIDStr(), std::move(stramptr));
  }
  repo_->MultiPut(flowData);
}

void ProvenanceReporterImpl::create(const core::FlowFile& flow_file, const std::string& detail) {
  auto event = allocate(ProvenanceEventRecord::CREATE, flow_file);

  if (event) {
    event->setDetails(detail);
    add(event);
  }
}

void ProvenanceReporterImpl::route(const core::FlowFile& flow_file, const core::Relationship& relation, const std::string& detail, std::chrono::milliseconds processingDuration) {
  auto event = allocate(ProvenanceEventRecord::ROUTE, flow_file);

  if (event) {
    event->setDetails(detail);
    event->setRelationship(relation.getName());
    event->setEventDuration(processingDuration);
    add(event);
  }
}

void ProvenanceReporterImpl::modifyAttributes(const core::FlowFile& flow_file, const std::string& detail) {
  auto event = allocate(ProvenanceEventRecord::ATTRIBUTES_MODIFIED, flow_file);

  if (event) {
    event->setDetails(detail);
    add(event);
  }
}

void ProvenanceReporterImpl::modifyContent(const core::FlowFile& flow_file, const std::string& detail, std::chrono::milliseconds processingDuration) {
  auto event = allocate(ProvenanceEventRecord::CONTENT_MODIFIED, flow_file);

  if (event) {
    event->setDetails(detail);
    event->setEventDuration(processingDuration);
    add(event);
  }
}

void ProvenanceReporterImpl::clone(const core::FlowFile& parent, const core::FlowFile& child) {
  auto event = allocate(ProvenanceEventRecord::CLONE, parent);

  if (event) {
    event->addChildFlowFile(child);
    event->addParentFlowFile(parent);
    add(event);
  }
}

void ProvenanceReporterImpl::expire(const core::FlowFile& flow_file, const std::string& detail) {
  auto event = allocate(ProvenanceEventRecord::EXPIRE, flow_file);

  if (event) {
    event->setDetails(detail);
    add(event);
  }
}

void ProvenanceReporterImpl::drop(const core::FlowFile& flow_file, const std::string& reason) {
  auto event = allocate(ProvenanceEventRecord::DROP, flow_file);

  if (event) {
    std::string dropReason = "Discard reason: " + reason;
    event->setDetails(dropReason);
    add(event);
  }
}

void ProvenanceReporterImpl::send(const core::FlowFile& flow_file, const std::string& transitUri, const std::string& detail, std::chrono::milliseconds processingDuration, bool force) {
  auto event = allocate(ProvenanceEventRecord::SEND, flow_file);

  if (event) {
    event->setTransitUri(transitUri);
    event->setDetails(detail);
    event->setEventDuration(processingDuration);
    if (!force) {
      add(event);
    } else {
      if (!repo_->isFull())
        repo_->storeElement(event);
    }
  }
}

void ProvenanceReporterImpl::receive(const core::FlowFile& flow_file,
                                 const std::string& transitUri,
                                 const std::string& sourceSystemFlowFileIdentifier,
                                 const std::string& detail,
                                 std::chrono::milliseconds processingDuration) {
  auto event = allocate(ProvenanceEventRecord::RECEIVE, flow_file);

  if (event) {
    event->setTransitUri(transitUri);
    event->setDetails(detail);
    event->setEventDuration(processingDuration);
    event->setSourceSystemFlowFileIdentifier(sourceSystemFlowFileIdentifier);
    add(event);
  }
}

void ProvenanceReporterImpl::fetch(const core::FlowFile& flow_file, const std::string& transitUri, const std::string& detail, std::chrono::milliseconds processingDuration) {
  auto event = allocate(ProvenanceEventRecord::FETCH, flow_file);

  if (event) {
    event->setTransitUri(transitUri);
    event->setDetails(detail);
    event->setEventDuration(processingDuration);
    add(event);
  }
}

}  // namespace org::apache::nifi::minifi::provenance
