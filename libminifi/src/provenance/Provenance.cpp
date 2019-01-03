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
#include "core/Repository.h"
#include "io/DataStream.h"
#include "io/Serializable.h"
#include "core/logging/Logger.h"
#include "core/Relationship.h"
#include "FlowController.h"

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
      _eventDuration(0),
      _entryDate(0),
      _lineageStartDate(0) {
  _eventType = event;
  _componentId = componentId;
  _componentType = componentType;
  _eventTime = getTimeMillis();
}

// DeSerialize
bool ProvenanceEventRecord::DeSerialize(const std::shared_ptr<core::SerializableComponent> &store) {
  std::string value;
  bool ret;

  const std::shared_ptr<core::Repository> repo = std::dynamic_pointer_cast<core::Repository>(store);

  if (nullptr == repo || IsNullOrEmpty(uuidStr_)) {
    logger_->log_error("Repo could not be assigned");
    return false;
  }
  ret = repo->Get(uuidStr_, value);

  if (!ret) {
    logger_->log_error("NiFi Provenance Store event %s can not be found", uuidStr_);
    return false;
  } else {
    logger_->log_debug("NiFi Provenance Read event %s", uuidStr_);
  }

  org::apache::nifi::minifi::io::DataStream stream((const uint8_t*) value.data(), value.length());

  ret = DeSerialize(stream);

  if (ret) {
    logger_->log_debug("NiFi Provenance retrieve event %s size %llu eventType %d success", uuidStr_, stream.getSize(), _eventType);
  } else {
    logger_->log_debug("NiFi Provenance retrieve event %s size %llu eventType %d fail", uuidStr_, stream.getSize(), _eventType);
  }

  return ret;
}

bool ProvenanceEventRecord::Serialize(const std::shared_ptr<core::SerializableComponent> &repo) {
  org::apache::nifi::minifi::io::DataStream outStream;

  int ret;

  ret = writeUTF(this->uuidStr_, &outStream);
  if (ret <= 0) {
    return false;
  }

  uint32_t eventType = this->_eventType;
  ret = write(eventType, &outStream);
  if (ret != 4) {
    return false;
  }

  ret = write(this->_eventTime, &outStream);
  if (ret != 8) {
    return false;
  }

  ret = write(this->_entryDate, &outStream);
  if (ret != 8) {
    return false;
  }

  ret = write(this->_eventDuration, &outStream);
  if (ret != 8) {
    return false;
  }

  ret = write(this->_lineageStartDate, &outStream);
  if (ret != 8) {
    return false;
  }

  ret = writeUTF(this->_componentId, &outStream);
  if (ret <= 0) {
    return false;
  }

  ret = writeUTF(this->_componentType, &outStream);
  if (ret <= 0) {
    return false;
  }

  ret = writeUTF(this->flow_uuid_, &outStream);
  if (ret <= 0) {
    return false;
  }

  ret = writeUTF(this->_details, &outStream);
  if (ret <= 0) {
    return false;
  }

  // write flow attributes
  uint32_t numAttributes = this->_attributes.size();
  ret = write(numAttributes, &outStream);
  if (ret != 4) {
    return false;
  }

  for (auto itAttribute : _attributes) {
    ret = writeUTF(itAttribute.first, &outStream, true);
    if (ret <= 0) {
      return false;
    }
    ret = writeUTF(itAttribute.second, &outStream, true);
    if (ret <= 0) {
      return false;
    }
  }

  ret = writeUTF(this->_contentFullPath, &outStream);
  if (ret <= 0) {
    return false;
  }

  ret = write(this->_size, &outStream);
  if (ret != 8) {
    return false;
  }

  ret = write(this->_offset, &outStream);
  if (ret != 8) {
    return false;
  }

  ret = writeUTF(this->_sourceQueueIdentifier, &outStream);
  if (ret <= 0) {
    return false;
  }

  if (this->_eventType == ProvenanceEventRecord::FORK || this->_eventType == ProvenanceEventRecord::CLONE || this->_eventType == ProvenanceEventRecord::JOIN) {
    // write UUIDs
    uint32_t number = this->_parentUuids.size();
    ret = write(number, &outStream);
    if (ret != 4) {
      return false;
    }
    for (auto parentUUID : _parentUuids) {
      ret = writeUTF(parentUUID, &outStream);
      if (ret <= 0) {
        return false;
      }
    }
    number = this->_childrenUuids.size();
    ret = write(number, &outStream);
    if (ret != 4) {
      return false;
    }
    for (auto childUUID : _childrenUuids) {
      ret = writeUTF(childUUID, &outStream);
      if (ret <= 0) {
        return false;
      }
    }
  } else if (this->_eventType == ProvenanceEventRecord::SEND || this->_eventType == ProvenanceEventRecord::FETCH) {
    ret = writeUTF(this->_transitUri, &outStream);
    if (ret <= 0) {
      return false;
    }
  } else if (this->_eventType == ProvenanceEventRecord::RECEIVE) {
    ret = writeUTF(this->_transitUri, &outStream);
    if (ret <= 0) {
      return false;
    }
    ret = writeUTF(this->_sourceSystemFlowFileIdentifier, &outStream);
    if (ret <= 0) {
      return false;
    }
  }
  // Persist to the DB
  if (!repo->Serialize(uuidStr_, const_cast<uint8_t*>(outStream.getBuffer()), outStream.getSize())) {
    logger_->log_error("NiFi Provenance Store event %s size %llu fail", uuidStr_, outStream.getSize());
  }
  return true;
}

bool ProvenanceEventRecord::DeSerialize(const uint8_t *buffer, const size_t bufferSize) {
  int ret;

  org::apache::nifi::minifi::io::DataStream outStream(buffer, bufferSize);

  ret = readUTF(this->uuidStr_, &outStream);

  if (ret <= 0) {
    return false;
  }

  uint32_t eventType;
  ret = read(eventType, &outStream);
  if (ret != 4) {
    return false;
  }
  this->_eventType = (ProvenanceEventRecord::ProvenanceEventType) eventType;

  ret = read(this->_eventTime, &outStream);
  if (ret != 8) {
    return false;
  }

  ret = read(this->_entryDate, &outStream);
  if (ret != 8) {
    return false;
  }

  ret = read(this->_eventDuration, &outStream);
  if (ret != 8) {
    return false;
  }

  ret = read(this->_lineageStartDate, &outStream);
  if (ret != 8) {
    return false;
  }

  ret = readUTF(this->_componentId, &outStream);
  if (ret <= 0) {
    return false;
  }

  ret = readUTF(this->_componentType, &outStream);
  if (ret <= 0) {
    return false;
  }

  ret = readUTF(this->flow_uuid_, &outStream);
  if (ret <= 0) {
    return false;
  }

  ret = readUTF(this->_details, &outStream);

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
    this->_attributes[key] = value;
  }

  ret = readUTF(this->_contentFullPath, &outStream);
  if (ret <= 0) {
    return false;
  }

  ret = read(this->_size, &outStream);
  if (ret != 8) {
    return false;
  }

  ret = read(this->_offset, &outStream);
  if (ret != 8) {
    return false;
  }

  ret = readUTF(this->_sourceQueueIdentifier, &outStream);
  if (ret <= 0) {
    return false;
  }

  if (this->_eventType == ProvenanceEventRecord::FORK || this->_eventType == ProvenanceEventRecord::CLONE || this->_eventType == ProvenanceEventRecord::JOIN) {
    // read UUIDs
    uint32_t number = 0;
    ret = read(number, &outStream);
    if (ret != 4) {
      return false;
    }

    for (uint32_t i = 0; i < number; i++) {
      std::string parentUUID;
      ret = readUTF(parentUUID, &outStream);
      if (ret <= 0) {
        return false;
      }
      this->addParentUuid(parentUUID);
    }
    number = 0;
    ret = read(number, &outStream);
    if (ret != 4) {
      return false;
    }
    for (uint32_t i = 0; i < number; i++) {
      std::string childUUID;
      ret = readUTF(childUUID, &outStream);
      if (ret <= 0) {
        return false;
      }
      this->addChildUuid(childUUID);
    }
  } else if (this->_eventType == ProvenanceEventRecord::SEND || this->_eventType == ProvenanceEventRecord::FETCH) {
    ret = readUTF(this->_transitUri, &outStream);
    if (ret <= 0) {
      return false;
    }
  } else if (this->_eventType == ProvenanceEventRecord::RECEIVE) {
    ret = readUTF(this->_transitUri, &outStream);
    if (ret <= 0) {
      return false;
    }
    ret = readUTF(this->_sourceSystemFlowFileIdentifier, &outStream);
    if (ret <= 0) {
      return false;
    }
  }

  return true;
}

void ProvenanceReporter::commit() {
  for (auto event : _events) {
    if (!repo_->isFull()) {
      event->Serialize(repo_);
    } else {
      logger_->log_debug("Provenance Repository is full");
    }
  }
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
