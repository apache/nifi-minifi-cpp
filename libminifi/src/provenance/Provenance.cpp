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

std::shared_ptr<utils::IdGenerator> ProvenanceEventRecord::id_generator_ = utils::IdGenerator::getIdGenerator();
std::shared_ptr<core::logging::Logger> ProvenanceEventRecord::logger_ = core::logging::LoggerFactory<ProvenanceEventRecord>::getLogger();

const char *ProvenanceEventRecord::ProvenanceEventTypeStr[REPLAY + 1] = { "CREATE", "RECEIVE", "FETCH", "SEND", "DOWNLOAD", "DROP", "EXPIRE", "FORK", "JOIN", "CLONE", "CONTENT_MODIFIED",
    "ATTRIBUTES_MODIFIED", "ROUTE", "ADDINFO", "REPLAY" };

ProvenanceEventRecord::ProvenanceEventRecord(ProvenanceEventRecord::ProvenanceEventType event, std::string componentId, std::string componentType)
    : core::SerializableComponent(core::getClassName<ProvenanceEventRecord>()),
      _eventType(event),
      _componentId(std::move(componentId)),
      _componentType(std::move(componentType)),
      _eventTime(std::chrono::system_clock::now()) {
}

bool ProvenanceEventRecord::loadFromRepository(const std::shared_ptr<core::Repository> &repo) {
  std::string value;
  bool ret;

  if (nullptr == repo || uuid_.isNil()) {
    logger_->log_error("Repo could not be assigned");
    return false;
  }
  ret = repo->Get(getUUIDStr(), value);

  if (!ret) {
    logger_->log_error("NiFi Provenance Store event %s can not be found", getUUIDStr());
    return false;
  } else {
    logger_->log_debug("NiFi Provenance Read event %s", getUUIDStr());
  }

  org::apache::nifi::minifi::io::BufferStream stream(value);

  ret = deserialize(stream);

  if (ret) {
    logger_->log_debug("NiFi Provenance retrieve event %s size %llu eventType %d success", getUUIDStr(), stream.size(), _eventType);
  } else {
    logger_->log_debug("NiFi Provenance retrieve event %s size %llu eventType %d fail", getUUIDStr(), stream.size(), _eventType);
  }

  return ret;
}

bool ProvenanceEventRecord::serialize(io::OutputStream& output_stream) {
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

bool ProvenanceEventRecord::deserialize(io::InputStream &input_stream) {
  {
    const auto ret = input_stream.read(uuid_);
    if (ret == 0 || io::isError(ret)) {
      return false;
    }
  }

  uint32_t eventType;
  {
    const auto ret = input_stream.read(eventType);
    if (ret != 4) {
      return false;
    }
  }

  this->_eventType = (ProvenanceEventRecord::ProvenanceEventType) eventType;
  {
    uint64_t event_time_in_ms;
    const auto ret = input_stream.read(event_time_in_ms);
    if (ret != 8) {
      return false;
    }
    _eventTime = std::chrono::system_clock::time_point() + std::chrono::milliseconds(event_time_in_ms);
  }

  {
    uint64_t entry_date_in_ms;
    const auto ret = input_stream.read(entry_date_in_ms);
    if (ret != 8) {
      return false;
    }
    _entryDate = std::chrono::system_clock::time_point() + std::chrono::milliseconds(entry_date_in_ms);
  }

  {
    uint64_t event_duration_ms;
    const auto ret = input_stream.read(event_duration_ms);
    if (ret != 8) {
      return false;
    }
    _eventDuration = std::chrono::milliseconds(event_duration_ms);
  }

  {
    uint64_t lineage_start_date_in_ms;
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

void ProvenanceReporter::commit() {
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

void ProvenanceReporter::create(const std::shared_ptr<core::FlowFile>& flow, const std::string& detail) {
  auto event = allocate(ProvenanceEventRecord::CREATE, flow);

  if (event) {
    event->setDetails(detail);
    add(event);
  }
}

void ProvenanceReporter::route(const std::shared_ptr<core::FlowFile>& flow, const core::Relationship& relation, const std::string& detail, std::chrono::milliseconds processingDuration) {
  auto event = allocate(ProvenanceEventRecord::ROUTE, flow);

  if (event) {
    event->setDetails(detail);
    event->setRelationship(relation.getName());
    event->setEventDuration(processingDuration);
    add(event);
  }
}

void ProvenanceReporter::modifyAttributes(const std::shared_ptr<core::FlowFile>& flow, const std::string& detail) {
  auto event = allocate(ProvenanceEventRecord::ATTRIBUTES_MODIFIED, flow);

  if (event) {
    event->setDetails(detail);
    add(event);
  }
}

void ProvenanceReporter::modifyContent(const std::shared_ptr<core::FlowFile>& flow, const std::string& detail, std::chrono::milliseconds processingDuration) {
  auto event = allocate(ProvenanceEventRecord::CONTENT_MODIFIED, flow);

  if (event) {
    event->setDetails(detail);
    event->setEventDuration(processingDuration);
    add(event);
  }
}

void ProvenanceReporter::clone(const std::shared_ptr<core::FlowFile>& parent, const std::shared_ptr<core::FlowFile>& child) {
  auto event = allocate(ProvenanceEventRecord::CLONE, parent);

  if (event) {
    event->addChildFlowFile(child);
    event->addParentFlowFile(parent);
    add(event);
  }
}

void ProvenanceReporter::join(const std::vector<std::shared_ptr<core::FlowFile>>& parents, const std::shared_ptr<core::FlowFile>& child,
    const std::string& detail, std::chrono::milliseconds processingDuration) {
  auto event = allocate(ProvenanceEventRecord::JOIN, child);

  if (event) {
    event->addChildFlowFile(child);
    for (const auto& parent : parents) {
      event->addParentFlowFile(parent);
    }
    event->setDetails(detail);
    event->setEventDuration(processingDuration);
    add(event);
  }
}

void ProvenanceReporter::fork(const std::vector<std::shared_ptr<core::FlowFile>>& children, const std::shared_ptr<core::FlowFile>& parent,
    const std::string& detail, std::chrono::milliseconds processingDuration) {
  auto event = allocate(ProvenanceEventRecord::FORK, parent);

  if (event) {
    event->addParentFlowFile(parent);
    for (const auto& child : children) {
      event->addChildFlowFile(child);
    }
    event->setDetails(detail);
    event->setEventDuration(processingDuration);
    add(event);
  }
}

void ProvenanceReporter::expire(const std::shared_ptr<core::FlowFile>& flow, const std::string& detail) {
  auto event = allocate(ProvenanceEventRecord::EXPIRE, flow);

  if (event) {
    event->setDetails(detail);
    add(event);
  }
}

void ProvenanceReporter::drop(const std::shared_ptr<core::FlowFile>& flow, const std::string& reason) {
  auto event = allocate(ProvenanceEventRecord::DROP, flow);

  if (event) {
    std::string dropReason = "Discard reason: " + reason;
    event->setDetails(dropReason);
    add(event);
  }
}

void ProvenanceReporter::send(const std::shared_ptr<core::FlowFile>& flow, const std::string& transitUri, const std::string& detail, std::chrono::milliseconds processingDuration, bool force) {
  auto event = allocate(ProvenanceEventRecord::SEND, flow);

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

void ProvenanceReporter::receive(const std::shared_ptr<core::FlowFile>& flow,
                                 const std::string& transitUri,
                                 const std::string& sourceSystemFlowFileIdentifier,
                                 const std::string& detail,
                                 std::chrono::milliseconds processingDuration) {
  auto event = allocate(ProvenanceEventRecord::RECEIVE, flow);

  if (event) {
    event->setTransitUri(transitUri);
    event->setDetails(detail);
    event->setEventDuration(processingDuration);
    event->setSourceSystemFlowFileIdentifier(sourceSystemFlowFileIdentifier);
    add(event);
  }
}

void ProvenanceReporter::fetch(const std::shared_ptr<core::FlowFile>& flow, const std::string& transitUri, const std::string& detail, std::chrono::milliseconds processingDuration) {
  auto event = allocate(ProvenanceEventRecord::FETCH, flow);

  if (event) {
    event->setTransitUri(transitUri);
    event->setDetails(detail);
    event->setEventDuration(processingDuration);
    add(event);
  }
}

}  // namespace org::apache::nifi::minifi::provenance
