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

#include <memory>
#include <string>
#include <utility>
#include <vector>
#include <queue>
#include <map>
#include <mutex>
#include <atomic>
#include <algorithm>
#include <set>
#include <unordered_map>
#include <unordered_set>

#include "minifi-cpp/core/ProcessContext.h"
#include "minifi-cpp/Exception.h"
#include "core/logging/LoggerFactory.h"
#include "FlowFile.h"
#include "WeakReference.h"
#include "provenance/Provenance.h"
#include "core/Relationship.h"
#include "minifi-cpp/utils/gsl.h"
#include "core/ProcessorMetrics.h"
#include "minifi-cpp/core/ProcessSession.h"

namespace org::apache::nifi::minifi::core::detail {

std::string to_string(const ReadBufferResult& read_buffer_result);

}  // namespace org::apache::nifi::minifi::core::detail

namespace org::apache::nifi::minifi::core {

class ProcessSessionImpl : public ReferenceContainerImpl, public virtual ProcessSession {
 public:
  explicit ProcessSessionImpl(std::shared_ptr<ProcessContext> processContext);

  ~ProcessSessionImpl() override;
  void commit() override;
  void rollback() override;

  nonstd::expected<void, std::string> rollbackNoThrow() noexcept override;

  std::shared_ptr<provenance::ProvenanceReporter> getProvenanceReporter() override {
    return provenance_report_;
  }

  void flushContent() override;

  std::shared_ptr<core::FlowFile> get() override;

  std::shared_ptr<core::FlowFile> create(const core::FlowFile* const parent = nullptr) override;
  void add(const std::shared_ptr<core::FlowFile> &record) override;
  std::shared_ptr<core::FlowFile> clone(const core::FlowFile& parent) override;
  std::shared_ptr<core::FlowFile> clone(const core::FlowFile& parent, int64_t offset, int64_t size) override;

  void transfer(const std::shared_ptr<core::IFlowFile>& flow, const Relationship& relationship) override;
  void transferToCustomRelationship(const std::shared_ptr<core::FlowFile>& flow, const std::string& relationship_name) override;

  void putAttribute(core::FlowFile& flow, std::string_view key, const std::string& value) override;
  void removeAttribute(core::FlowFile& flow, std::string_view key) override;

  void remove(const std::shared_ptr<core::FlowFile> &flow) override;

  std::shared_ptr<io::InputStream> getFlowFileContentStream(const core::FlowFile& flow_file) override;

  int64_t read(const std::shared_ptr<core::FlowFile>& flow_file, const io::InputStreamCallback& callback) override;

  int64_t read(const core::FlowFile& flow_file, const io::InputStreamCallback& callback) override;

  detail::ReadBufferResult readBuffer(const std::shared_ptr<core::FlowFile>& flow) override;

  void write(const std::shared_ptr<core::FlowFile> &flow, const io::OutputStreamCallback& callback) override;

  void write(core::FlowFile& flow, const io::OutputStreamCallback& callback) override;
  // Read and write the flow file at the same time (eg. for processing it line by line)
  int64_t readWrite(const std::shared_ptr<core::FlowFile> &flow, const io::InputOutputStreamCallback& callback) override;

  void writeBuffer(const std::shared_ptr<core::FlowFile>& flow_file, std::span<const char> buffer) override;
  void writeBuffer(const std::shared_ptr<core::FlowFile>& flow_file, std::span<const std::byte> buffer) override;

  void append(const std::shared_ptr<core::FlowFile> &flow, const io::OutputStreamCallback& callback) override;

  void appendBuffer(const std::shared_ptr<core::FlowFile>& flow, std::span<const char> buffer) override;
  void appendBuffer(const std::shared_ptr<core::FlowFile>& flow, std::span<const std::byte> buffer) override;

  void penalize(const std::shared_ptr<core::FlowFile> &flow) override;

  bool outgoingConnectionsFull(const std::string& relationship) override;

  void importFrom(io::InputStream &stream, const std::shared_ptr<core::FlowFile> &flow) override;
  void importFrom(io::InputStream&& stream, const std::shared_ptr<core::FlowFile> &flow) override;

  void import(const std::string& source, const std::shared_ptr<core::FlowFile> &flow, bool keepSource = true, uint64_t offset = 0) override;
  void import(const std::string& source, std::vector<std::shared_ptr<FlowFile>> &flows, uint64_t offset, char inputDelimiter) override;
  void import(const std::string& source, std::vector<std::shared_ptr<FlowFile>> &flows, bool keepSource, uint64_t offset, char inputDelimiter) override;

  bool exportContent(const std::string &destination, const std::shared_ptr<core::FlowFile> &flow, bool keepContent) override;

  bool exportContent(const std::string &destination, const std::string &tmpFileName, const std::shared_ptr<core::FlowFile> &flow, bool keepContent) override;

  void stash(const std::string &key, const std::shared_ptr<core::FlowFile> &flow) override;

  void restore(const std::string &key, const std::shared_ptr<core::FlowFile> &flow) override;

  bool existsFlowFileInRelationship(const Relationship &relationship) override;

  void setMetrics(const std::shared_ptr<ProcessorMetrics>& metrics) {
    metrics_ = metrics;
  }

  bool hasBeenTransferred(const core::FlowFile &flow) const override;

  ProcessSessionImpl(const ProcessSessionImpl &parent) = delete;
  ProcessSessionImpl &operator=(const ProcessSessionImpl &parent) = delete;

 protected:
  struct FlowFileUpdate {
    std::shared_ptr<FlowFile> modified;
    std::shared_ptr<FlowFile> snapshot;
  };

  using Relationships = std::unordered_set<Relationship>;

  Relationships relationships_;

  struct NewFlowFileInfo {
    std::shared_ptr<core::FlowFile> flow_file;
    const Relationship* rel{nullptr};
  };

  // FlowFiles being modified by current process session
  std::map<utils::Identifier, FlowFileUpdate> updated_flowfiles_;
  // updated FlowFiles being transferred to the relationship
  std::map<utils::Identifier, const Relationship*> updated_relationships_;
  // FlowFiles being added by current process session
  std::map<utils::Identifier, NewFlowFileInfo> added_flowfiles_;
  // FlowFiles being deleted by current process session
  std::vector<std::shared_ptr<core::FlowFile>> deleted_flowfiles_;
  // FlowFiles being cloned for multiple connections per relationship
  std::vector<std::shared_ptr<core::FlowFile>> cloned_flowfiles_;

 private:
  enum class RouteResult {
    Ok_Routed,
    Ok_AutoTerminated,
    Ok_Deleted,
    Error_NoRelationship
  };

  struct TransferMetrics {
    size_t transfer_count = 0;
    uint64_t transfer_size = 0;
  };

  RouteResult routeFlowFile(const std::shared_ptr<FlowFile>& record, const std::function<void(const FlowFile&, const Relationship&)>& transfer_callback);

  void persistFlowFilesBeforeTransfer(
      std::map<Connectable*, std::vector<std::shared_ptr<core::FlowFile>>>& transactionMap,
      const std::map<utils::Identifier, FlowFileUpdate>& modifiedFlowFiles);

  void ensureNonNullResourceClaim(
      const std::map<Connectable*, std::vector<std::shared_ptr<core::FlowFile>>>& transactionMap);

  std::shared_ptr<core::FlowFile> cloneDuringTransfer(const core::FlowFile& parent);
  std::shared_ptr<ProcessContext> process_context_;
  std::shared_ptr<logging::Logger> logger_;
  std::shared_ptr<provenance::ProvenanceReporter> provenance_report_;
  std::shared_ptr<ContentSession> content_session_;
  StateManager* stateManager_;

  static std::shared_ptr<utils::IdGenerator> id_generator_;

  std::shared_ptr<ProcessorMetrics> metrics_;
};

}  // namespace org::apache::nifi::minifi::core
