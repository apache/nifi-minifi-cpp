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
#ifndef LIBMINIFI_INCLUDE_PROVENANCE_PROVENANCE_H_
#define LIBMINIFI_INCLUDE_PROVENANCE_PROVENANCE_H_

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
// Provenance Event Record Serialization Seg Size
#define PROVENANCE_EVENT_RECORD_SEG_SIZE 2048

// Provenance Event Record
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

  // Destructor
  virtual ~ProvenanceEventRecord() = default;
  // Get the Event ID
  utils::Identifier getEventId() {
    return getUUID();
  }

  void setEventId(const utils::Identifier &id) {
    setUUID(id);
  }
  // Get Attributes
  std::map<std::string, std::string> getAttributes() {
    return _attributes;
  }
  // Get Size
  uint64_t getFileSize() {
    return _size;
  }
  // ! Get Offset
  uint64_t getFileOffset() {
    return _offset;
  }
  // ! Get Entry Date
  std::chrono::system_clock::time_point getFlowFileEntryDate() {
    return _entryDate;
  }
  // ! Get Lineage Start Date
  std::chrono::system_clock::time_point getlineageStartDate() {
    return _lineageStartDate;
  }
  // ! Get Event Time
  std::chrono::system_clock::time_point getEventTime() {
    return _eventTime;
  }
  // ! Get Event Duration
  std::chrono::milliseconds getEventDuration() {
    return _eventDuration;
  }
  // Set Event Duration
  void setEventDuration(std::chrono::milliseconds duration) {
    _eventDuration = duration;
  }
  // ! Get Event Type
  ProvenanceEventType getEventType() {
    return _eventType;
  }
  // Get Component ID
  std::string getComponentId() {
    return _componentId;
  }
  // Get Component Type
  std::string getComponentType() {
    return _componentType;
  }
  // Get FlowFileUuid
  utils::Identifier getFlowFileUuid() {
    return flow_uuid_;
  }
  // Get content full path
  std::string getContentFullPath() {
    return _contentFullPath;
  }
  // Get LineageIdentifiers
  std::vector<utils::Identifier> getLineageIdentifiers() {
    return _lineageIdentifiers;
  }
  // Get Details
  std::string getDetails() {
    return _details;
  }
  // Set Details
  void setDetails(std::string details) {
    _details = details;
  }
  // Get TransitUri
  std::string getTransitUri() {
    return _transitUri;
  }
  // Set TransitUri
  void setTransitUri(std::string uri) {
    _transitUri = uri;
  }
  // Get SourceSystemFlowFileIdentifier
  std::string getSourceSystemFlowFileIdentifier() {
    return _sourceSystemFlowFileIdentifier;
  }
  // Set SourceSystemFlowFileIdentifier
  void setSourceSystemFlowFileIdentifier(std::string identifier) {
    _sourceSystemFlowFileIdentifier = identifier;
  }
  // Get Parent UUIDs
  std::vector<utils::Identifier> getParentUuids() {
    return _parentUuids;
  }
  // Add Parent UUID
  void addParentUuid(const utils::Identifier& uuid) {
    if (std::find(_parentUuids.begin(), _parentUuids.end(), uuid) != _parentUuids.end())
      return;
    else
      _parentUuids.push_back(uuid);
  }
  // Add Parent Flow File
  void addParentFlowFile(const std::shared_ptr<core::FlowFile>& flow) {
    addParentUuid(flow->getUUID());
  }
  // Remove Parent UUID
  void removeParentUuid(const utils::Identifier& uuid) {
    _parentUuids.erase(std::remove(_parentUuids.begin(), _parentUuids.end(), uuid), _parentUuids.end());
  }
  // Remove Parent Flow File
  void removeParentFlowFile(const std::shared_ptr<core::FlowFile>& flow) {
    removeParentUuid(flow->getUUID());
  }
  // Get Children UUIDs
  std::vector<utils::Identifier> getChildrenUuids() {
    return _childrenUuids;
  }
  // Add Child UUID
  void addChildUuid(const utils::Identifier& uuid) {
    if (std::find(_childrenUuids.begin(), _childrenUuids.end(), uuid) != _childrenUuids.end())
      return;
    else
      _childrenUuids.push_back(uuid);
  }
  // Add Child Flow File
  void addChildFlowFile(std::shared_ptr<core::FlowFile> flow) {
    addChildUuid(flow->getUUID());
    return;
  }
  // Remove Child UUID
  void removeChildUuid(const utils::Identifier& uuid) {
    _childrenUuids.erase(std::remove(_childrenUuids.begin(), _childrenUuids.end(), uuid), _childrenUuids.end());
  }
  // Remove Child Flow File
  void removeChildFlowFile(std::shared_ptr<core::FlowFile> flow) {
    removeChildUuid(flow->getUUID());
  }
  // Get AlternateIdentifierUri
  std::string getAlternateIdentifierUri() {
    return _alternateIdentifierUri;
  }
  // Set AlternateIdentifierUri
  void setAlternateIdentifierUri(std::string uri) {
    _alternateIdentifierUri = uri;
  }
  // Get Relationship
  std::string getRelationship() {
    return _relationship;
  }
  // Set Relationship
  void setRelationship(std::string relation) {
    _relationship = relation;
  }
  // Get sourceQueueIdentifier
  std::string getSourceQueueIdentifier() {
    return _sourceQueueIdentifier;
  }
  // Set sourceQueueIdentifier
  void setSourceQueueIdentifier(std::string identifier) {
    _sourceQueueIdentifier = identifier;
  }
  // fromFlowFile
  void fromFlowFile(std::shared_ptr<core::FlowFile> &flow) {
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
  using SerializableComponent::Serialize;

  // Serialize the event to a stream
  bool Serialize(org::apache::nifi::minifi::io::BufferStream& outStream);

  // Serialize and Persistent to the repository
  bool Serialize(const std::shared_ptr<core::SerializableComponent> &repo) override;
  bool DeSerialize(gsl::span<const std::byte>) override;
  bool DeSerialize(org::apache::nifi::minifi::io::BufferStream &stream) {
    return DeSerialize(stream.getBuffer());
  }
  bool DeSerialize(const std::shared_ptr<core::SerializableComponent> &store) override;

 protected:
  // Event type
  ProvenanceEventType _eventType;
  // Date at which the event was created
  std::chrono::system_clock::time_point _eventTime{};
  // Date at which the flow file entered the flow
  std::chrono::system_clock::time_point _entryDate{};
  // Date at which the origin of this flow file entered the flow
  std::chrono::system_clock::time_point _lineageStartDate{};
  // Event Duration
  std::chrono::milliseconds _eventDuration{};
  // Component ID
  std::string _componentId;
  // Component Type
  std::string _componentType;
  // Size in bytes of the data corresponding to this flow file
  uint64_t _size;
  // flow uuid
  utils::Identifier flow_uuid_;
  // Offset to the content
  uint64_t _offset;
  // Full path to the content
  std::string _contentFullPath;
  // Attributes key/values pairs for the flow record
  std::map<std::string, std::string> _attributes;
  // UUID string for all parents
  std::vector<utils::Identifier> _lineageIdentifiers;
  // transitUri
  std::string _transitUri;
  // sourceSystemFlowFileIdentifier
  std::string _sourceSystemFlowFileIdentifier;
  // parent UUID
  std::vector<utils::Identifier> _parentUuids;
  // child UUID
  std::vector<utils::Identifier> _childrenUuids;
  // detail
  std::string _details;
  // sourceQueueIdentifier
  std::string _sourceQueueIdentifier;
  // relationship
  std::string _relationship;
  // alternateIdentifierUri;
  std::string _alternateIdentifierUri;

 private:
  // Prevent default copy constructor and assignment operation
  // Only support pass by reference or pointer
  ProvenanceEventRecord(const ProvenanceEventRecord &parent);
  ProvenanceEventRecord &operator=(const ProvenanceEventRecord &parent);
  static std::shared_ptr<core::logging::Logger> logger_;
  static std::shared_ptr<utils::IdGenerator> id_generator_;
};

// Provenance Reporter
class ProvenanceReporter {
 public:
  // Constructor
  /*!
   * Create a new provenance reporter associated with the process session
   */
  ProvenanceReporter(std::shared_ptr<core::Repository> repo, std::string componentId, std::string componentType)
      : logger_(core::logging::LoggerFactory<ProvenanceReporter>::getLogger()) {
    _componentId = componentId;
    _componentType = componentType;
    repo_ = repo;
  }

  // Destructor
  virtual ~ProvenanceReporter() {
    clear();
  }
  // Get events
  std::set<std::shared_ptr<ProvenanceEventRecord>> getEvents() {
    return _events;
  }
  // Add event
  void add(const std::shared_ptr<ProvenanceEventRecord> &event) {
    _events.insert(event);
  }
  // Remove event
  void remove(const std::shared_ptr<ProvenanceEventRecord> &event) {
    if (_events.find(event) != _events.end()) {
      _events.erase(event);
    }
  }
  //
  // clear
  void clear() {
    _events.clear();
  }
  // commit
  void commit();
  // create
  void create(std::shared_ptr<core::FlowFile> flow, std::string detail);
  // route
  void route(std::shared_ptr<core::FlowFile> flow, core::Relationship relation, std::string detail, std::chrono::milliseconds processingDuration);
  // modifyAttributes
  void modifyAttributes(std::shared_ptr<core::FlowFile> flow, std::string detail);
  // modifyContent
  void modifyContent(std::shared_ptr<core::FlowFile> flow, std::string detail, std::chrono::milliseconds processingDuration);
  // clone
  void clone(std::shared_ptr<core::FlowFile> parent, std::shared_ptr<core::FlowFile> child);
  // join
  void join(std::vector<std::shared_ptr<core::FlowFile> > parents, std::shared_ptr<core::FlowFile> child, std::string detail, std::chrono::milliseconds processingDuration);
  // fork
  void fork(std::vector<std::shared_ptr<core::FlowFile> > child, std::shared_ptr<core::FlowFile> parent, std::string detail, std::chrono::milliseconds processingDuration);
  // expire
  void expire(std::shared_ptr<core::FlowFile> flow, std::string detail);
  // drop
  void drop(std::shared_ptr<core::FlowFile> flow, std::string reason);
  // send
  void send(std::shared_ptr<core::FlowFile> flow, std::string transitUri, std::string detail, std::chrono::milliseconds processingDuration, bool force);
  // fetch
  void fetch(std::shared_ptr<core::FlowFile> flow, std::string transitUri, std::string detail, std::chrono::milliseconds processingDuration);
  // receive
  void receive(std::shared_ptr<core::FlowFile> flow, std::string transitUri, std::string sourceSystemFlowFileIdentifier, std::string detail, std::chrono::milliseconds processingDuration);

 protected:
  // allocate
  std::shared_ptr<ProvenanceEventRecord> allocate(ProvenanceEventRecord::ProvenanceEventType eventType, std::shared_ptr<core::FlowFile> flow) {
    if (repo_->isNoop()) {
      return nullptr;
    }

    auto event = std::make_shared<ProvenanceEventRecord>(eventType, _componentId, _componentType);
    if (event)
      event->fromFlowFile(flow);

    return event;
  }

  // Component ID
  std::string _componentId;
  // Component Type
  std::string _componentType;

 private:
  std::shared_ptr<core::logging::Logger> logger_;
  // Incoming connection Iterator
  std::set<std::shared_ptr<ProvenanceEventRecord>> _events;
  // provenance repository.
  std::shared_ptr<core::Repository> repo_;

  // Prevent default copy constructor and assignment operation
  // Only support pass by reference or pointer
  ProvenanceReporter(const ProvenanceReporter &parent);
  ProvenanceReporter &operator=(const ProvenanceReporter &parent);
};

// Provenance Repository

}  // namespace org::apache::nifi::minifi::provenance

#endif  // LIBMINIFI_INCLUDE_PROVENANCE_PROVENANCE_H_
