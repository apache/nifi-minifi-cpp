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

namespace org::apache::nifi::minifi::provenance {

class ProvenanceEventRecord : public core::SerializableComponent {
 public:
  enum ProvenanceEventType {
    /**
     * A CREATE event is used when a FlowFile is generated from data that was
     * not received from a remote system or external process
     */
    CREATE,

    /**
     * Indicates a provenance event for receiving data from an external process. This Event Type
     * is expected to be the first event for a FlowFile. As such, a Processor that receives data
     * from an external source and uses that data to replace the content of an existing FlowFile
     * should use the {@link #FETCH} event type, rather than the RECEIVE event type.
     */
    RECEIVE,

    /**
     * Indicates that the contents of a FlowFile were overwritten using the contents of some
     * external resource. This is similar to the {@link #RECEIVE} event but varies in that
     * RECEIVE events are intended to be used as the event that introduces the FlowFile into
     * the system, whereas FETCH is used to indicate that the contents of an existing FlowFile
     * were overwritten.
     */
    FETCH,

    /**
     * Indicates a provenance event for sending data to an external process
     */
    SEND,

    /**
     * Indicates that the contents of a FlowFile were downloaded by a user or external entity.
     */
    DOWNLOAD,

    /**
     * Indicates a provenance event for the conclusion of an object's life for
     * some reason other than object expiration
     */
    DROP,

    /**
     * Indicates a provenance event for the conclusion of an object's life due
     * to the fact that the object could not be processed in a timely manner
     */
    EXPIRE,

    /**
     * FORK is used to indicate that one or more FlowFile was derived from a
     * parent FlowFile.
     */
    FORK,

    /**
     * JOIN is used to indicate that a single FlowFile is derived from joining
     * together multiple parent FlowFiles.
     */
    JOIN,

    /**
     * CLONE is used to indicate that a FlowFile is an exact duplicate of its
     * parent FlowFile.
     */
    CLONE,

    /**
     * CONTENT_MODIFIED is used to indicate that a FlowFile's content was
     * modified in some way. When using this Event Type, it is advisable to
     * provide details about how the content is modified.
     */
    CONTENT_MODIFIED,

    /**
     * ATTRIBUTES_MODIFIED is used to indicate that a FlowFile's attributes were
     * modified in some way. This event is not needed when another event is
     * reported at the same time, as the other event will already contain all
     * FlowFile attributes.
     */
    ATTRIBUTES_MODIFIED,

    /**
     * ROUTE is used to show that a FlowFile was routed to a specified
     * {@link org.apache.nifi.processor.Relationship Relationship} and should provide
     * information about why the FlowFile was routed to this relationship.
     */
    ROUTE,

    /**
     * Indicates a provenance event for adding additional information such as a
     * new linkage to a new URI or UUID
     */
    ADDINFO,

    /**
     * Indicates a provenance event for replaying a FlowFile. The UUID of the
     * event will indicate the UUID of the original FlowFile that is being
     * replayed. The event will contain exactly one Parent UUID that is also the
     * UUID of the FlowFile that is being replayed and exactly one Child UUID
     * that is the UUID of the a newly created FlowFile that will be re-queued
     * for processing.
     */
    REPLAY
  };
  static const char *ProvenanceEventTypeStr[REPLAY + 1];

  ProvenanceEventRecord(ProvenanceEventType event, std::string componentId, std::string componentType);

  ProvenanceEventRecord()
      : core::SerializableComponent(core::getClassName<ProvenanceEventRecord>()) {
    _eventTime = std::chrono::system_clock::now();
  }

  virtual ~ProvenanceEventRecord() = default;

  utils::Identifier getEventId() const {
    return getUUID();
  }

  void setEventId(const utils::Identifier &id) {
    setUUID(id);
  }

  std::map<std::string, std::string> getAttributes() const {
    return _attributes;
  }

  uint64_t getFileSize() const {
    return _size;
  }

  uint64_t getFileOffset() const {
    return _offset;
  }

  std::chrono::system_clock::time_point getFlowFileEntryDate() const {
    return _entryDate;
  }

  std::chrono::system_clock::time_point getlineageStartDate() const {
    return _lineageStartDate;
  }

  std::chrono::system_clock::time_point getEventTime() const {
    return _eventTime;
  }

  std::chrono::milliseconds getEventDuration() const {
    return _eventDuration;
  }

  void setEventDuration(std::chrono::milliseconds duration) {
    _eventDuration = duration;
  }

  ProvenanceEventType getEventType() const {
    return _eventType;
  }

  std::string getComponentId() const {
    return _componentId;
  }

  std::string getComponentType() const {
    return _componentType;
  }

  utils::Identifier getFlowFileUuid() const {
    return flow_uuid_;
  }

  std::string getContentFullPath() const {
    return _contentFullPath;
  }

  std::vector<utils::Identifier> getLineageIdentifiers() const {
    return _lineageIdentifiers;
  }

  std::string getDetails() const {
    return _details;
  }

  void setDetails(const std::string& details) {
    _details = details;
  }

  std::string getTransitUri() {
    return _transitUri;
  }

  void setTransitUri(const std::string& uri) {
    _transitUri = uri;
  }

  std::string getSourceSystemFlowFileIdentifier() const {
    return _sourceSystemFlowFileIdentifier;
  }

  void setSourceSystemFlowFileIdentifier(const std::string& identifier) {
    _sourceSystemFlowFileIdentifier = identifier;
  }

  std::vector<utils::Identifier> getParentUuids() const {
    return _parentUuids;
  }

  void addParentUuid(const utils::Identifier& uuid) {
    if (std::find(_parentUuids.begin(), _parentUuids.end(), uuid) != _parentUuids.end())
      return;
    else
      _parentUuids.push_back(uuid);
  }

  void addParentFlowFile(const std::shared_ptr<core::FlowFile>& flow) {
    addParentUuid(flow->getUUID());
  }

  void removeParentUuid(const utils::Identifier& uuid) {
    _parentUuids.erase(std::remove(_parentUuids.begin(), _parentUuids.end(), uuid), _parentUuids.end());
  }

  void removeParentFlowFile(const std::shared_ptr<core::FlowFile>& flow) {
    removeParentUuid(flow->getUUID());
  }

  std::vector<utils::Identifier> getChildrenUuids() const {
    return _childrenUuids;
  }

  void addChildUuid(const utils::Identifier& uuid) {
    if (std::find(_childrenUuids.begin(), _childrenUuids.end(), uuid) != _childrenUuids.end())
      return;
    else
      _childrenUuids.push_back(uuid);
  }

  void addChildFlowFile(const std::shared_ptr<core::FlowFile>& flow) {
    addChildUuid(flow->getUUID());
    return;
  }

  void removeChildUuid(const utils::Identifier& uuid) {
    _childrenUuids.erase(std::remove(_childrenUuids.begin(), _childrenUuids.end(), uuid), _childrenUuids.end());
  }

  void removeChildFlowFile(const std::shared_ptr<core::FlowFile>& flow) {
    removeChildUuid(flow->getUUID());
  }

  std::string getAlternateIdentifierUri() const {
    return _alternateIdentifierUri;
  }

  void setAlternateIdentifierUri(const std::string& uri) {
    _alternateIdentifierUri = uri;
  }

  std::string getRelationship() const {
    return _relationship;
  }

  void setRelationship(const std::string& relation) {
    _relationship = relation;
  }

  std::string getSourceQueueIdentifier() const {
    return _sourceQueueIdentifier;
  }

  void setSourceQueueIdentifier(const std::string& identifier) {
    _sourceQueueIdentifier = identifier;
  }

  void fromFlowFile(const std::shared_ptr<core::FlowFile> &flow) {
    _entryDate = flow->getEntryDate();
    _lineageStartDate = flow->getlineageStartDate();
    _lineageIdentifiers = flow->getlineageIdentifiers();
    flow_uuid_ = flow->getUUID();
    _attributes = flow->getAttributes();
    _size = flow->getSize();
    _offset = flow->getOffset();
    if (flow->getConnection())
      _sourceQueueIdentifier = flow->getConnection()->getName();
    if (flow->getResourceClaim()) {
      _contentFullPath = flow->getResourceClaim()->getContentFullPath();
    }
  }

  bool serialize(io::OutputStream& output_stream) override;
  bool deserialize(io::InputStream &input_stream) override;
  bool loadFromRepository(const std::shared_ptr<core::Repository> &repo);

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
  uint64_t _size;
  utils::Identifier flow_uuid_;
  uint64_t _offset;
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
  ProvenanceEventRecord(const ProvenanceEventRecord &parent);
  ProvenanceEventRecord &operator=(const ProvenanceEventRecord &parent);
  static std::shared_ptr<core::logging::Logger> logger_;
  static std::shared_ptr<utils::IdGenerator> id_generator_;
};

class ProvenanceReporter {
 public:
  ProvenanceReporter(std::shared_ptr<core::Repository> repo, std::string componentId, std::string componentType)
      : logger_(core::logging::LoggerFactory<ProvenanceReporter>::getLogger()) {
    _componentId = componentId;
    _componentType = componentType;
    repo_ = repo;
  }

  virtual ~ProvenanceReporter() {
    clear();
  }

  std::set<std::shared_ptr<ProvenanceEventRecord>> getEvents() const {
    return _events;
  }

  void add(const std::shared_ptr<ProvenanceEventRecord> &event) {
    _events.insert(event);
  }

  void remove(const std::shared_ptr<ProvenanceEventRecord> &event) {
    if (_events.find(event) != _events.end()) {
      _events.erase(event);
    }
  }

  void clear() {
    _events.clear();
  }

  void commit();
  void create(const std::shared_ptr<core::FlowFile>& flow, const std::string& detail);
  void route(const std::shared_ptr<core::FlowFile>& flow, const core::Relationship& relation, const std::string& detail, std::chrono::milliseconds processingDuration);
  void modifyAttributes(const std::shared_ptr<core::FlowFile>& flow, const std::string& detail);
  void modifyContent(const std::shared_ptr<core::FlowFile>& flow, const std::string& detail, std::chrono::milliseconds processingDuration);
  void clone(const std::shared_ptr<core::FlowFile>& parent, const std::shared_ptr<core::FlowFile>& child);
  void join(const std::vector<std::shared_ptr<core::FlowFile>>& parents, const std::shared_ptr<core::FlowFile>& child, const std::string& detail, std::chrono::milliseconds processingDuration);
  void fork(const std::vector<std::shared_ptr<core::FlowFile>>& children, const std::shared_ptr<core::FlowFile>& parent, const std::string& detail, std::chrono::milliseconds processingDuration);
  void expire(const std::shared_ptr<core::FlowFile>& flow, const std::string& detail);
  void drop(const std::shared_ptr<core::FlowFile>& flow, const std::string& reason);
  void send(const std::shared_ptr<core::FlowFile>& flow, const std::string& transitUri, const std::string& detail, std::chrono::milliseconds processingDuration, bool force);
  void fetch(const std::shared_ptr<core::FlowFile>& flow, const std::string& transitUri, const std::string& detail, std::chrono::milliseconds processingDuration);
  void receive(const std::shared_ptr<core::FlowFile>& flow, const std::string& transitUri,
    const std::string& sourceSystemFlowFileIdentifier, const std::string& detail, std::chrono::milliseconds processingDuration);

 protected:
  std::shared_ptr<ProvenanceEventRecord> allocate(ProvenanceEventRecord::ProvenanceEventType eventType, const std::shared_ptr<core::FlowFile>& flow) {
    if (repo_->isNoop()) {
      return nullptr;
    }

    auto event = std::make_shared<ProvenanceEventRecord>(eventType, _componentId, _componentType);
    if (event)
      event->fromFlowFile(flow);

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
  ProvenanceReporter(const ProvenanceReporter &parent);
  ProvenanceReporter &operator=(const ProvenanceReporter &parent);
};

}  // namespace org::apache::nifi::minifi::provenance
