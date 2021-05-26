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

#include "core/Repository.h"
#include "io/BufferStream.h"
#include "core/logging/Logger.h"
#include "core/Relationship.h"
#include "FlowController.h"
#include "utils/gsl.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace provenance {

std::shared_ptr<utils::IdGenerator> ProvenanceEventRecord::id_generator_ = utils::IdGenerator::getIdGenerator();
std::shared_ptr<logging::Logger> ProvenanceEventRecord::logger_ = logging::LoggerFactory<ProvenanceEventRecord>::getLogger();

const char *ProvenanceEventRecord::ProvenanceEventTypeStr[REPLAY + 1] = { "CREATE", "RECEIVE", "FETCH", "SEND", "DOWNLOAD", "DROP", "EXPIRE", "FORK", "JOIN", "CLONE", "CONTENT_MODIFIED",
    "ATTRIBUTES_MODIFIED", "ROUTE", "ADDINFO", "REPLAY" };

ProvenanceEventRecord::ProvenanceEventRecord(ProvenanceEventRecord::ProvenanceEventType event, std::string componentId, std::string componentType)
    : core::SerializableComponent(core::getClassName<ProvenanceEventRecord>()),
      _entryDate(0),
      _lineageStartDate(0),
      _eventDuration(0) {
  _eventType = event;
  _componentId = componentId;
  _componentType = componentType;
  _eventTime = utils::timeutils::getTimeMillis();
}

// DeSerialize
bool ProvenanceEventRecord::DeSerialize(const std::shared_ptr<core::SerializableComponent> &store) {
  std::string value;
  bool ret;

  const std::shared_ptr<core::Repository> repo = std::dynamic_pointer_cast<core::Repository>(store);

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

  org::apache::nifi::minifi::io::BufferStream stream((const uint8_t*) value.data(), gsl::narrow<int>(value.length()));

  ret = DeSerialize(stream);

  if (ret) {
    logger_->log_debug("NiFi Provenance retrieve event %s size %llu eventType %d success", getUUIDStr(), stream.size(), _eventType);
  } else {
    logger_->log_debug("NiFi Provenance retrieve event %s size %llu eventType %d fail", getUUIDStr(), stream.size(), _eventType);
  }

  return ret;
}

bool ProvenanceEventRecord::Serialize(org::apache::nifi::minifi::io::BufferStream& outStream) {
  int ret;

  ret = outStream.write(this->uuid_);
  if (ret <= 0) {
    return false;
  }

  uint32_t eventType = this->_eventType;
  ret = outStream.write(eventType);
  if (ret != 4) {
    return false;
  }

  ret = outStream.write(this->_eventTime);
  if (ret != 8) {
    return false;
  }

  ret = outStream.write(this->_entryDate);
  if (ret != 8) {
    return false;
  }

  ret = outStream.write(this->_eventDuration);
  if (ret != 8) {
    return false;
  }

  ret = outStream.write(this->_lineageStartDate);
  if (ret != 8) {
    return false;
  }

  ret = outStream.write(this->_componentId);
  if (ret <= 0) {
    return false;
  }

  ret = outStream.write(this->_componentType);
  if (ret <= 0) {
    return false;
  }

  ret = outStream.write(this->flow_uuid_);
  if (ret <= 0) {
    return false;
  }

  ret = outStream.write(this->_details);
  if (ret <= 0) {
    return false;
  }

  // write flow attributes
  uint32_t numAttributes = gsl::narrow<uint32_t>(this->_attributes.size());
  ret = outStream.write(numAttributes);
  if (ret != 4) {
    return false;
  }

  for (const auto& itAttribute : _attributes) {
    ret = outStream.write(itAttribute.first);
    if (ret <= 0) {
      return false;
    }
    ret = outStream.write(itAttribute.second);
    if (ret <= 0) {
      return false;
    }
  }

  ret = outStream.write(this->_contentFullPath);
  if (ret <= 0) {
    return false;
  }

  ret = outStream.write(this->_size);
  if (ret != 8) {
    return false;
  }

  ret = outStream.write(this->_offset);
  if (ret != 8) {
    return false;
  }

  ret = outStream.write(this->_sourceQueueIdentifier);
  if (ret <= 0) {
    return false;
  }

  if (this->_eventType == ProvenanceEventRecord::FORK || this->_eventType == ProvenanceEventRecord::CLONE || this->_eventType == ProvenanceEventRecord::JOIN) {
    // write UUIDs
    uint32_t parent_uuids_count = gsl::narrow<uint32_t>(this->_parentUuids.size());
    ret = outStream.write(parent_uuids_count);
    if (ret != 4) {
      return false;
    }
    for (const auto& parentUUID : _parentUuids) {
      ret = outStream.write(parentUUID);
      if (ret <= 0) {
        return false;
      }
    }
    uint32_t children_uuids_count = gsl::narrow<uint32_t>(this->_childrenUuids.size());
    ret = outStream.write(children_uuids_count);
    if (ret != 4) {
      return false;
    }
    for (const auto& childUUID : _childrenUuids) {
      ret = outStream.write(childUUID);
      if (ret <= 0) {
        return false;
      }
    }
  } else if (this->_eventType == ProvenanceEventRecord::SEND || this->_eventType == ProvenanceEventRecord::FETCH) {
    ret = outStream.write(this->_transitUri);
    if (ret <= 0) {
      return false;
    }
  } else if (this->_eventType == ProvenanceEventRecord::RECEIVE) {
    ret = outStream.write(this->_transitUri);
    if (ret <= 0) {
      return false;
    }
    ret = outStream.write(this->_sourceSystemFlowFileIdentifier);
    if (ret <= 0) {
      return false;
    }
  }

  return true;
}

bool ProvenanceEventRecord::Serialize(const std::shared_ptr<core::SerializableComponent> &repo) {
  org::apache::nifi::minifi::io::BufferStream outStream;

  Serialize(outStream);

  // Persist to the DB
  if (!repo->Serialize(getUUIDStr(), const_cast<uint8_t*>(outStream.getBuffer()), outStream.size())) {
    logger_->log_error("NiFi Provenance Store event %s size %llu fail", getUUIDStr(), outStream.size());
  }
  return true;
}

bool ProvenanceEventRecord::DeSerialize(const uint8_t *buffer, const size_t bufferSize) {
  org::apache::nifi::minifi::io::BufferStream outStream(buffer, gsl::narrow<unsigned int>(bufferSize));

  {
    const auto ret = outStream.read(uuid_);
    if (ret == 0 || io::isError(ret)) {
      return false;
    }
  }

  uint32_t eventType;
  {
    const auto ret = outStream.read(eventType);
    if (ret != 4) {
      return false;
    }
  }

  this->_eventType = (ProvenanceEventRecord::ProvenanceEventType) eventType;
  {
    const auto ret = outStream.read(this->_eventTime);
    if (ret != 8) {
      return false;
    }
  }

  {
    const auto ret = outStream.read(this->_entryDate);
    if (ret != 8) {
      return false;
    }
  }

  {
    const auto ret = outStream.read(this->_eventDuration);
    if (ret != 8) {
      return false;
    }
  }

  {
    const auto ret = outStream.read(this->_lineageStartDate);
    if (ret != 8) {
      return false;
    }
  }

  {
    const auto ret = outStream.read(this->_componentId);
    if (ret == 0 || io::isError(ret)) {
      return false;
    }
  }

  {
    const auto ret = outStream.read(this->_componentType);
    if (ret == 0 || io::isError(ret)) {
      return false;
    }
  }

  {
    const auto ret = outStream.read(this->flow_uuid_);
    if (ret == 0 || io::isError(ret)) {
      return false;
    }
  }

  {
    const auto ret = outStream.read(this->_details);
    if (ret == 0 || io::isError(ret)) {
      return false;
    }
  }

  // read flow attributes
  uint32_t numAttributes = 0;
  {
    const auto ret = outStream.read(numAttributes);
    if (ret != 4) {
      return false;
    }
  }

  for (uint32_t i = 0; i < numAttributes; i++) {
    std::string key;
    {
      const auto ret = outStream.read(key);
      if (ret == 0 || io::isError(ret)) {
        return false;
      }
    }
    std::string value;
    {
      const auto ret = outStream.read(value);
      if (ret == 0 || io::isError(ret)) {
        return false;
      }
    }
    this->_attributes[key] = value;
  }

  {
    const auto ret = outStream.read(this->_contentFullPath);
    if (ret == 0 || io::isError(ret)) {
      return false;
    }
  }

  {
    const auto ret = outStream.read(this->_size);
    if (ret != 8) {
      return false;
    }
  }

  {
    const auto ret = outStream.read(this->_offset);
    if (ret != 8) {
      return false;
    }
  }

  {
    const auto ret = outStream.read(this->_sourceQueueIdentifier);
    if (ret == 0 || io::isError(ret)) {
      return false;
    }
  }

  if (this->_eventType == ProvenanceEventRecord::FORK || this->_eventType == ProvenanceEventRecord::CLONE || this->_eventType == ProvenanceEventRecord::JOIN) {
    // read UUIDs
    uint32_t number = 0;
    {
      const auto ret = outStream.read(number);
      if (ret != 4) {
        return false;
      }
    }

    for (uint32_t i = 0; i < number; i++) {
      utils::Identifier parentUUID;
      {
        const auto ret = outStream.read(parentUUID);
        if (ret == 0 || io::isError(ret)) {
          return false;
        }
      }
      this->addParentUuid(parentUUID);
    }
    number = 0;
    {
      const auto ret = outStream.read(number);
      if (ret != 4) {
        return false;
      }
    }
    for (uint32_t i = 0; i < number; i++) {
      utils::Identifier childUUID;
      {
        const auto ret = outStream.read(childUUID);
        if (ret == 0 || io::isError(ret)) {
          return false;
        }
      }
      this->addChildUuid(childUUID);
    }
  } else if (this->_eventType == ProvenanceEventRecord::SEND || this->_eventType == ProvenanceEventRecord::FETCH) {
    {
      const auto ret = outStream.read(this->_transitUri);
      if (ret == 0 || io::isError(ret)) {
        return false;
      }
    }
  } else if (this->_eventType == ProvenanceEventRecord::RECEIVE) {
    {
      const auto ret = outStream.read(this->_transitUri);
      if (ret == 0 || io::isError(ret)) {
        return false;
      }
    }
    {
      const auto ret = outStream.read(this->_sourceSystemFlowFileIdentifier);
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
    event->Serialize(*stramptr.get());

    flowData.emplace_back(event->getUUIDStr(), std::move(stramptr));
  }
  repo_->MultiPut(flowData);
}

void ProvenanceReporter::create(std::shared_ptr<core::FlowFile> flow, std::string detail) {
  auto event = allocate(ProvenanceEventRecord::CREATE, flow);

  if (event) {
    event->setDetails(detail);
    add(event);
  }
}

void ProvenanceReporter::route(std::shared_ptr<core::FlowFile> flow, core::Relationship relation, std::string detail, uint64_t processingDuration) {
  auto event = allocate(ProvenanceEventRecord::ROUTE, flow);

  if (event) {
    event->setDetails(detail);
    event->setRelationship(relation.getName());
    event->setEventDuration(processingDuration);
    add(event);
  }
}

void ProvenanceReporter::modifyAttributes(std::shared_ptr<core::FlowFile> flow, std::string detail) {
  auto event = allocate(ProvenanceEventRecord::ATTRIBUTES_MODIFIED, flow);

  if (event) {
    event->setDetails(detail);
    add(event);
  }
}

void ProvenanceReporter::modifyContent(std::shared_ptr<core::FlowFile> flow, std::string detail, uint64_t processingDuration) {
  auto event = allocate(ProvenanceEventRecord::CONTENT_MODIFIED, flow);

  if (event) {
    event->setDetails(detail);
    event->setEventDuration(processingDuration);
    add(event);
  }
}

void ProvenanceReporter::clone(std::shared_ptr<core::FlowFile> parent, std::shared_ptr<core::FlowFile> child) {
  auto event = allocate(ProvenanceEventRecord::CLONE, parent);

  if (event) {
    event->addChildFlowFile(child);
    event->addParentFlowFile(parent);
    add(event);
  }
}

void ProvenanceReporter::join(std::vector<std::shared_ptr<core::FlowFile> > parents, std::shared_ptr<core::FlowFile> child, std::string detail, uint64_t processingDuration) {
  auto event = allocate(ProvenanceEventRecord::JOIN, child);

  if (event) {
    event->addChildFlowFile(child);
    std::vector<std::shared_ptr<core::FlowFile> >::iterator it;
    for (it = parents.begin(); it != parents.end(); it++) {
      std::shared_ptr<core::FlowFile> record = *it;
      event->addParentFlowFile(record);
    }
    event->setDetails(detail);
    event->setEventDuration(processingDuration);
    add(event);
  }
}

void ProvenanceReporter::fork(std::vector<std::shared_ptr<core::FlowFile> > child, std::shared_ptr<core::FlowFile> parent, std::string detail, uint64_t processingDuration) {
  auto event = allocate(ProvenanceEventRecord::FORK, parent);

  if (event) {
    event->addParentFlowFile(parent);
    std::vector<std::shared_ptr<core::FlowFile> >::iterator it;
    for (it = child.begin(); it != child.end(); it++) {
      std::shared_ptr<core::FlowFile> record = *it;
      event->addChildFlowFile(record);
    }
    event->setDetails(detail);
    event->setEventDuration(processingDuration);
    add(event);
  }
}

void ProvenanceReporter::expire(std::shared_ptr<core::FlowFile> flow, std::string detail) {
  auto event = allocate(ProvenanceEventRecord::EXPIRE, flow);

  if (event) {
    event->setDetails(detail);
    add(event);
  }
}

void ProvenanceReporter::drop(std::shared_ptr<core::FlowFile> flow, std::string reason) {
  auto event = allocate(ProvenanceEventRecord::DROP, flow);

  if (event) {
    std::string dropReason = "Discard reason: " + reason;
    event->setDetails(dropReason);
    add(event);
  }
}

void ProvenanceReporter::send(std::shared_ptr<core::FlowFile> flow, std::string transitUri, std::string detail, uint64_t processingDuration, bool force) {
  auto event = allocate(ProvenanceEventRecord::SEND, flow);

  if (event) {
    event->setTransitUri(transitUri);
    event->setDetails(detail);
    event->setEventDuration(processingDuration);
    if (!force) {
      add(event);
    } else {
      if (!repo_->isFull())
        event->Serialize(repo_);
    }
  }
}

void ProvenanceReporter::receive(std::shared_ptr<core::FlowFile> flow, std::string transitUri, std::string sourceSystemFlowFileIdentifier, std::string detail, uint64_t processingDuration) {
  auto event = allocate(ProvenanceEventRecord::RECEIVE, flow);

  if (event) {
    event->setTransitUri(transitUri);
    event->setDetails(detail);
    event->setEventDuration(processingDuration);
    event->setSourceSystemFlowFileIdentifier(sourceSystemFlowFileIdentifier);
    add(event);
  }
}

void ProvenanceReporter::fetch(std::shared_ptr<core::FlowFile> flow, std::string transitUri, std::string detail, uint64_t processingDuration) {
  auto event = allocate(ProvenanceEventRecord::FETCH, flow);

  if (event) {
    event->setTransitUri(transitUri);
    event->setDetails(detail);
    event->setEventDuration(processingDuration);
    add(event);
  }
}

} /* namespace provenance */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
