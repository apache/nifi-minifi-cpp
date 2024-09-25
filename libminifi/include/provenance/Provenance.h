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
#pragma once

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <thread>
#include <vector>
#include "core/Core.h"
#include "core/SerializableComponent.h"
#include "core/Repository.h"
#include "core/Property.h"
#include "properties/Configure.h"
#include "Connection.h"
#include "FlowFileRecord.h"
#include "core/logging/LoggerFactory.h"
#include "ResourceClaim.h"
#include "utils/gsl.h"
#include "utils/Id.h"
#include "utils/TimeUtil.h"
#include "minifi-cpp/provenance/Provenance.h"

namespace org::apache::nifi::minifi::provenance {

class ProvenanceEventRecordImpl : public core::SerializableComponentImpl, public virtual ProvenanceEventRecord {
 public:
  static const char *ProvenanceEventTypeStr[REPLAY + 1];

  ProvenanceEventRecordImpl(ProvenanceEventType event, std::string componentId, std::string componentType);

  ProvenanceEventRecordImpl()
      : core::SerializableComponentImpl(core::className<ProvenanceEventRecord>()) {
    _eventTime = std::chrono::system_clock::now();
  }

  ~ProvenanceEventRecordImpl() override = default;

  utils::Identifier getEventId() const override {
    return getUUID();
  }

  void setEventId(const utils::Identifier &id) override {
    setUUID(id);
  }

  std::map<std::string, std::string> getAttributes() const override {
    return _attributes;
  }

  uint64_t getFileSize() const override {
    return _size;
  }

  uint64_t getFileOffset() const override {
    return _offset;
  }

  std::chrono::system_clock::time_point getFlowFileEntryDate() const override {
    return _entryDate;
  }

  std::chrono::system_clock::time_point getlineageStartDate() const override {
    return _lineageStartDate;
  }

  std::chrono::system_clock::time_point getEventTime() const override {
    return _eventTime;
  }

  std::chrono::milliseconds getEventDuration() const override {
    return _eventDuration;
  }

  void setEventDuration(std::chrono::milliseconds duration) override {
    _eventDuration = duration;
  }

  ProvenanceEventType getEventType() const override {
    return _eventType;
  }

  std::string getComponentId() const override {
    return _componentId;
  }

  std::string getComponentType() const override {
    return _componentType;
  }

  utils::Identifier getFlowFileUuid() const override {
    return flow_uuid_;
  }

  std::string getContentFullPath() const override {
    return _contentFullPath;
  }

  std::vector<utils::Identifier> getLineageIdentifiers() const override {
    return _lineageIdentifiers;
  }

  std::string getDetails() const override {
    return _details;
  }

  void setDetails(const std::string& details) override {
    _details = details;
  }

  std::string getTransitUri() override {
    return _transitUri;
  }

  void setTransitUri(const std::string& uri) override {
    _transitUri = uri;
  }

  std::string getSourceSystemFlowFileIdentifier() const override {
    return _sourceSystemFlowFileIdentifier;
  }

  void setSourceSystemFlowFileIdentifier(const std::string& identifier) override {
    _sourceSystemFlowFileIdentifier = identifier;
  }

  std::vector<utils::Identifier> getParentUuids() const override {
    return _parentUuids;
  }

  void addParentUuid(const utils::Identifier& uuid) override {
    if (std::find(_parentUuids.begin(), _parentUuids.end(), uuid) != _parentUuids.end())
      return;
    else
      _parentUuids.push_back(uuid);
  }

  void addParentFlowFile(const core::FlowFile& flow_file) override {
    addParentUuid(flow_file.getUUID());
  }

  void removeParentUuid(const utils::Identifier& uuid) override {
    _parentUuids.erase(std::remove(_parentUuids.begin(), _parentUuids.end(), uuid), _parentUuids.end());
  }

  void removeParentFlowFile(const core::FlowFile& flow_file) override {
    removeParentUuid(flow_file.getUUID());
  }

  std::vector<utils::Identifier> getChildrenUuids() const override {
    return _childrenUuids;
  }

  void addChildUuid(const utils::Identifier& uuid) override {
    if (std::find(_childrenUuids.begin(), _childrenUuids.end(), uuid) != _childrenUuids.end())
      return;
    else
      _childrenUuids.push_back(uuid);
  }

  void addChildFlowFile(const core::FlowFile& flow_file) override {
    addChildUuid(flow_file.getUUID());
    return;
  }

  void removeChildUuid(const utils::Identifier& uuid) override {
    _childrenUuids.erase(std::remove(_childrenUuids.begin(), _childrenUuids.end(), uuid), _childrenUuids.end());
  }

  void removeChildFlowFile(const core::FlowFile& flow_file) override {
    removeChildUuid(flow_file.getUUID());
  }

  std::string getAlternateIdentifierUri() const override {
    return _alternateIdentifierUri;
  }

  void setAlternateIdentifierUri(const std::string& uri) override {
    _alternateIdentifierUri = uri;
  }

  std::string getRelationship() const override {
    return _relationship;
  }

  void setRelationship(const std::string& relation) override {
    _relationship = relation;
  }

  std::string getSourceQueueIdentifier() const override {
    return _sourceQueueIdentifier;
  }

  void setSourceQueueIdentifier(const std::string& identifier) override {
    _sourceQueueIdentifier = identifier;
  }

  void fromFlowFile(const core::FlowFile& flow_file) override {
    _entryDate = flow_file.getEntryDate();
    _lineageStartDate = flow_file.getlineageStartDate();
    _lineageIdentifiers = flow_file.getlineageIdentifiers();
    flow_uuid_ = flow_file.getUUID();
    _attributes = flow_file.getAttributes();
    _size = flow_file.getSize();
    _offset = flow_file.getOffset();
    if (flow_file.getConnection())
      _sourceQueueIdentifier = flow_file.getConnection()->getName();
    if (flow_file.getResourceClaim()) {
      _contentFullPath = flow_file.getResourceClaim()->getContentFullPath();
    }
  }

  bool serialize(io::OutputStream& output_stream) override;
  bool deserialize(io::InputStream &input_stream) override;
  bool loadFromRepository(const std::shared_ptr<core::Repository> &repo) override;

 protected:
  ProvenanceEventType _eventType;
  // Date at which the event was created
  std::chrono::system_clock::time_point _eventTime{};
  // Date at which the flow file entered the flow
  std::chrono::system_clock::time_point _entryDate{};
  // Date at which the origin of this flow file entered the flow
  std::chrono::system_clock::time_point _lineageStartDate{};
  std::chrono::milliseconds _eventDuration{};
  std::string _componentId;
  std::string _componentType;
  // Size in bytes of the data corresponding to this flow file
  uint64_t _size = 0;
  utils::Identifier flow_uuid_;
  uint64_t _offset = 0;
  std::string _contentFullPath;
  std::map<std::string, std::string> _attributes;
  // UUID string for all parents
  std::vector<utils::Identifier> _lineageIdentifiers;
  std::string _transitUri;
  std::string _sourceSystemFlowFileIdentifier;
  std::vector<utils::Identifier> _parentUuids;
  std::vector<utils::Identifier> _childrenUuids;
  std::string _details;
  std::string _sourceQueueIdentifier;
  std::string _relationship;
  std::string _alternateIdentifierUri;

 private:
  // Prevent default copy constructor and assignment operation
  // Only support pass by reference or pointer
  ProvenanceEventRecordImpl(const ProvenanceEventRecordImpl &parent);
  ProvenanceEventRecordImpl &operator=(const ProvenanceEventRecordImpl &parent);
  static std::shared_ptr<core::logging::Logger> logger_;
  static std::shared_ptr<utils::IdGenerator> id_generator_;
};

class ProvenanceReporterImpl : public virtual ProvenanceReporter {
 public:
  ProvenanceReporterImpl(std::shared_ptr<core::Repository> repo, std::string componentId, std::string componentType)
      : logger_(core::logging::LoggerFactory<ProvenanceReporter>::getLogger()) {
    _componentId = componentId;
    _componentType = componentType;
    repo_ = repo;
  }

  ~ProvenanceReporterImpl() override {
    clear();
  }

  std::set<std::shared_ptr<ProvenanceEventRecord>> getEvents() const override {
    return _events;
  }

  void add(const std::shared_ptr<ProvenanceEventRecord> &event) override {
    _events.insert(event);
  }

  void remove(const std::shared_ptr<ProvenanceEventRecord> &event) override {
    if (_events.find(event) != _events.end()) {
      _events.erase(event);
    }
  }

  void clear() override {
    _events.clear();
  }

  void commit() override;
  void create(const core::FlowFile& flow_file, const std::string& detail) override;
  void route(const core::FlowFile& flow_file, const core::Relationship& relation, const std::string& detail, std::chrono::milliseconds processingDuration) override;
  void modifyAttributes(const core::FlowFile& flow_file, const std::string& detail) override;
  void modifyContent(const core::FlowFile& flow_file, const std::string& detail, std::chrono::milliseconds processingDuration) override;
  void clone(const core::FlowFile& parent, const core::FlowFile& child) override;
  void expire(const core::FlowFile& flow_file, const std::string& detail) override;
  void drop(const core::FlowFile& flow_file, const std::string& reason) override;
  void send(const core::FlowFile& flow_file, const std::string& transitUri, const std::string& detail, std::chrono::milliseconds processingDuration, bool force) override;
  void fetch(const core::FlowFile& flow_file, const std::string& transitUri, const std::string& detail, std::chrono::milliseconds processingDuration) override;
  void receive(const core::FlowFile& flow_file, const std::string& transitUri,
    const std::string& sourceSystemFlowFileIdentifier, const std::string& detail, std::chrono::milliseconds processingDuration) override;

 protected:
  std::shared_ptr<ProvenanceEventRecord> allocate(ProvenanceEventRecord::ProvenanceEventType eventType, const core::FlowFile& flow_file) {
    if (repo_->isNoop()) {
      return nullptr;
    }

    auto event = std::make_shared<ProvenanceEventRecordImpl>(eventType, _componentId, _componentType);
    if (event)
      event->fromFlowFile(flow_file);

    return event;
  }

  std::string _componentId;
  std::string _componentType;

 private:
  std::shared_ptr<core::logging::Logger> logger_;
  std::set<std::shared_ptr<ProvenanceEventRecord>> _events;
  std::shared_ptr<core::Repository> repo_;

  // Prevent default copy constructor and assignment operation
  // Only support pass by reference or pointer
  ProvenanceReporterImpl(const ProvenanceReporterImpl &parent);
  ProvenanceReporterImpl &operator=(const ProvenanceReporterImpl &parent);
};

}  // namespace org::apache::nifi::minifi::provenance
