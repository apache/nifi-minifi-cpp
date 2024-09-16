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
#include "minifi-cpp/core/Core.h"
#include "minifi-cpp/core/SerializableComponent.h"
#include "minifi-cpp/core/Repository.h"
#include "minifi-cpp/core/Property.h"
#include "minifi-cpp/properties/Configure.h"
#include "minifi-cpp/Connection.h"
#include "minifi-cpp/ResourceClaim.h"
#include "utils/gsl.h"
#include "utils/Id.h"
#include "utils/TimeUtil.h"

namespace org::apache::nifi::minifi::provenance {

class ProvenanceEventRecord : public virtual core::SerializableComponent {
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

  ~ProvenanceEventRecord() override = default;

  virtual utils::Identifier getEventId() const = 0;
  virtual void setEventId(const utils::Identifier &id) = 0;
  virtual std::map<std::string, std::string> getAttributes() const = 0;
  virtual uint64_t getFileSize() const = 0;
  virtual uint64_t getFileOffset() const = 0;
  virtual std::chrono::system_clock::time_point getFlowFileEntryDate() const = 0;
  virtual std::chrono::system_clock::time_point getlineageStartDate() const = 0;
  virtual std::chrono::system_clock::time_point getEventTime() const = 0;
  virtual std::chrono::milliseconds getEventDuration() const = 0;
  virtual void setEventDuration(std::chrono::milliseconds duration) = 0;
  virtual ProvenanceEventType getEventType() const = 0;
  virtual std::string getComponentId() const = 0;
  virtual std::string getComponentType() const = 0;
  virtual utils::Identifier getFlowFileUuid() const = 0;
  virtual std::string getContentFullPath() const = 0;
  virtual std::vector<utils::Identifier> getLineageIdentifiers() const = 0;
  virtual std::string getDetails() const = 0;
  virtual void setDetails(const std::string& details) = 0;
  virtual std::string getTransitUri() = 0;
  virtual void setTransitUri(const std::string& uri) = 0;
  virtual std::string getSourceSystemFlowFileIdentifier() const = 0;
  virtual void setSourceSystemFlowFileIdentifier(const std::string& identifier) = 0;
  virtual std::vector<utils::Identifier> getParentUuids() const = 0;
  virtual void addParentUuid(const utils::Identifier& uuid) = 0;
  virtual void addParentFlowFile(const core::FlowFile& flow_file) = 0;
  virtual void removeParentUuid(const utils::Identifier& uuid) = 0;
  virtual void removeParentFlowFile(const core::FlowFile& flow_file) = 0;
  virtual std::vector<utils::Identifier> getChildrenUuids() const = 0;
  virtual void addChildUuid(const utils::Identifier& uuid) = 0;
  virtual void addChildFlowFile(const core::FlowFile& flow_file) = 0;
  virtual void removeChildUuid(const utils::Identifier& uuid) = 0;
  virtual void removeChildFlowFile(const core::FlowFile& flow_file) = 0;
  virtual std::string getAlternateIdentifierUri() const = 0;
  virtual void setAlternateIdentifierUri(const std::string& uri) = 0;
  virtual std::string getRelationship() const = 0;
  virtual void setRelationship(const std::string& relation) = 0;
  virtual std::string getSourceQueueIdentifier() const = 0;
  virtual void setSourceQueueIdentifier(const std::string& identifier) = 0;
  virtual void fromFlowFile(const core::FlowFile& flow_file) = 0;
  virtual bool loadFromRepository(const std::shared_ptr<core::Repository> &repo) = 0;
};

class ProvenanceReporter {
 public:
  virtual ~ProvenanceReporter() = default;

  virtual std::set<std::shared_ptr<ProvenanceEventRecord>> getEvents() const = 0;
  virtual void add(const std::shared_ptr<ProvenanceEventRecord> &event) = 0 ;
  virtual void remove(const std::shared_ptr<ProvenanceEventRecord> &event) = 0;
  virtual void clear() = 0;

  virtual void commit() = 0;
  virtual void create(const core::FlowFile& flow_file, const std::string& detail) = 0;
  virtual void route(const core::FlowFile& flow_file, const core::Relationship& relation, const std::string& detail, std::chrono::milliseconds processingDuration) = 0;
  virtual void modifyAttributes(const core::FlowFile& flow_file, const std::string& detail) = 0;
  virtual void modifyContent(const core::FlowFile& flow_file, const std::string& detail, std::chrono::milliseconds processingDuration) = 0;
  virtual void clone(const core::FlowFile& parent, const core::FlowFile& child) = 0;
  virtual void expire(const core::FlowFile& flow_file, const std::string& detail) = 0;
  virtual void drop(const core::FlowFile& flow_file, const std::string& reason) = 0;
  virtual void send(const core::FlowFile& flow_file, const std::string& transitUri, const std::string& detail, std::chrono::milliseconds processingDuration, bool force) = 0;
  virtual void fetch(const core::FlowFile& flow_file, const std::string& transitUri, const std::string& detail, std::chrono::milliseconds processingDuration) = 0;
  virtual void receive(const core::FlowFile& flow_file, const std::string& transitUri,
    const std::string& sourceSystemFlowFileIdentifier, const std::string& detail, std::chrono::milliseconds processingDuration) = 0;
};

}  // namespace org::apache::nifi::minifi::provenance
