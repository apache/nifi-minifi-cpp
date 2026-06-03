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
#include "minifi-cpp/core/Repository.h"
#include "minifi-cpp/core/Property.h"
#include "properties/Configure.h"
#include "core/logging/LoggerFactory.h"
#include "minifi-cpp/utils/gsl.h"
#include "utils/Id.h"
#include "utils/TimeUtil.h"
#include "minifi-cpp/provenance/Provenance.h"

namespace org::apache::nifi::minifi::provenance {

class ProvenanceEventRecordImpl : public core::SerializableComponentImpl, public virtual ProvenanceEventRecord {
 public:
  static const char *ProvenanceEventTypeStr[REPLAY + 1];

  ProvenanceEventRecordImpl(ProvenanceEventType event, utils::Identifier component_id, std::string component_type);

  ProvenanceEventRecordImpl(const ProvenanceEventRecordImpl&) = delete;
  ProvenanceEventRecordImpl(ProvenanceEventRecordImpl&&) = delete;
  ProvenanceEventRecordImpl& operator=(const ProvenanceEventRecordImpl&) = delete;
  ProvenanceEventRecordImpl& operator=(ProvenanceEventRecordImpl&&) = delete;

  ~ProvenanceEventRecordImpl() override = default;

  utils::Identifier getEventId() const override {
    return getUUID();
  }

  void setEventId(const utils::Identifier &id) override {
    setUUID(id);
  }

  std::map<std::string, std::string> getAttributes() const override {
    return attributes_;
  }

  uint64_t getFileSize() const override {
    return size_;
  }

  uint64_t getFileOffset() const override {
    return offset_;
  }

  std::chrono::system_clock::time_point getFlowFileEntryDate() const override {
    return entry_date_;
  }

  std::chrono::system_clock::time_point getLineageStartDate() const override {
    return lineage_start_date_;
  }

  std::chrono::system_clock::time_point getEventTime() const override {
    return event_time_;
  }

  std::chrono::milliseconds getEventDuration() const override {
    return event_duration_;
  }

  void setEventDuration(std::chrono::milliseconds duration) override {
    event_duration_ = duration;
  }

  ProvenanceEventType getEventType() const override {
    return event_type_;
  }

  std::string getComponentId() const override {
    return component_id_;
  }

  std::string getComponentType() const override {
    return component_type_;
  }

  utils::Identifier getFlowFileUuid() const override {
    return flow_uuid_;
  }

  std::string getContentFullPath() const override {
    return content_full_path;
  }

  std::vector<utils::Identifier> getLineageIdentifiers() const override {
    return lineage_identifiers;
  }

  std::string getDetails() const override {
    return details_;
  }

  void setDetails(const std::string& details) override {
    details_ = details;
  }

  std::string getTransitUri() override {
    return transit_uri_;
  }

  void setTransitUri(const std::string& uri) override {
    transit_uri_ = uri;
  }

  std::string getSourceSystemFlowFileIdentifier() const override {
    return source_system_flow_file_identifier_;
  }

  void setSourceSystemFlowFileIdentifier(const std::string& identifier) override {
    source_system_flow_file_identifier_ = identifier;
  }

  std::vector<utils::Identifier> getParentUuids() const override {
    return parent_uuids_;
  }

  void addParentUuid(const utils::Identifier& uuid) override {
    if (std::find(parent_uuids_.begin(), parent_uuids_.end(), uuid) != parent_uuids_.end())
      return;
    else
      parent_uuids_.push_back(uuid);
  }

  void addParentFlowFile(const core::FlowFile& flow_file) override {
    addParentUuid(flow_file.getUUID());
  }

  std::vector<utils::Identifier> getChildrenUuids() const override {
    return children_uuids_;
  }

  void addChildUuid(const utils::Identifier& uuid) override {
    if (std::find(children_uuids_.begin(), children_uuids_.end(), uuid) != children_uuids_.end())
      return;
    else
      children_uuids_.push_back(uuid);
  }

  void addChildFlowFile(const core::FlowFile& flow_file) override {
    addChildUuid(flow_file.getUUID());
    return;
  }

  std::string getAlternateIdentifierUri() const override {
    return alternate_identifier_uri_;
  }

  void setAlternateIdentifierUri(const std::string& uri) override {
    alternate_identifier_uri_ = uri;
  }

  std::string getRelationship() const override {
    return relationship_;
  }

  void setRelationship(const std::string& relation) override {
    relationship_ = relation;
  }

  std::string getSourceQueueIdentifier() const override {
    return source_queue_identifier_;
  }

  void setSourceQueueIdentifier(const std::string& identifier) override {
    source_queue_identifier_ = identifier;
  }

  void fromFlowFile(const core::FlowFile& flow_file) override {
    entry_date_ = flow_file.getEntryDate();
    lineage_start_date_ = flow_file.getLineageStartDate();
    lineage_identifiers = flow_file.getlineageIdentifiers();
    flow_uuid_ = flow_file.getUUID();
    attributes_ = flow_file.getAttributes();
    size_ = flow_file.getSize();
    offset_ = flow_file.getOffset();
    if (flow_file.getConnection())
      source_queue_identifier_ = flow_file.getConnection()->getName();
    if (flow_file.getResourceClaim()) {
      content_full_path = flow_file.getResourceClaim()->getContentFullPath();
    }
  }

  bool serialize(io::OutputStream& output_stream) override;
  bool deserialize(io::InputStream &input_stream) override;
  bool loadFromRepository(const std::shared_ptr<core::Repository> &repo) override;

 protected:
  ProvenanceEventType event_type_;
  // Date at which the event was created
  std::chrono::system_clock::time_point event_time_{};
  // Date at which the flow file entered the flow
  std::chrono::system_clock::time_point entry_date_{};
  // Date at which the origin of this flow file entered the flow
  std::chrono::system_clock::time_point lineage_start_date_{};
  std::chrono::milliseconds event_duration_{};
  std::string component_id_;
  std::string component_type_;
  // Size in bytes of the data corresponding to this flow file
  uint64_t size_ = 0;
  utils::Identifier flow_uuid_;
  uint64_t offset_ = 0;
  std::string content_full_path;
  std::map<std::string, std::string> attributes_;
  // UUID string for all parents
  std::vector<utils::Identifier> lineage_identifiers;
  std::string transit_uri_;
  std::string source_system_flow_file_identifier_;
  std::vector<utils::Identifier> parent_uuids_;
  std::vector<utils::Identifier> children_uuids_;
  std::string details_;
  std::string source_queue_identifier_;
  std::string relationship_;
  std::string alternate_identifier_uri_;

 private:
  static std::shared_ptr<core::logging::Logger> logger_;
  static std::shared_ptr<utils::IdGenerator> id_generator_;
};

class ProvenanceReporterImpl : public virtual ProvenanceReporter {
 public:
  ProvenanceReporterImpl(std::shared_ptr<core::Repository> repo, utils::Identifier component_id, std::string component_type)
      : component_id_(component_id),
        component_type_(std::move(component_type)),
        logger_(core::logging::LoggerFactory<ProvenanceReporter>::getLogger()),
        repo_(std::move(repo)) {}

  ProvenanceReporterImpl(const ProvenanceReporterImpl&) = delete;
  ProvenanceReporterImpl(ProvenanceReporterImpl&&) = delete;
  ProvenanceReporterImpl& operator=(const ProvenanceReporterImpl&) = delete;
  ProvenanceReporterImpl& operator=(ProvenanceReporterImpl&&) = delete;

  ~ProvenanceReporterImpl() override {
    clear();
  }

  std::set<std::shared_ptr<ProvenanceEventRecord>> getEvents() const override {
    return events_;
  }

  void add(const std::shared_ptr<ProvenanceEventRecord> &event) override {
    events_.insert(event);
  }

  void remove(const std::shared_ptr<ProvenanceEventRecord> &event) override {
    events_.erase(event);
  }

  void clear() final {
    events_.clear();
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

    auto event = std::make_shared<ProvenanceEventRecordImpl>(eventType, component_id_, component_type_);
    if (event)
      event->fromFlowFile(flow_file);

    return event;
  }

  utils::Identifier component_id_;
  std::string component_type_;

 private:
  std::shared_ptr<core::logging::Logger> logger_;
  std::set<std::shared_ptr<ProvenanceEventRecord>> events_;
  std::shared_ptr<core::Repository> repo_;
};

}  // namespace org::apache::nifi::minifi::provenance
