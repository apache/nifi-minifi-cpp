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

#include "ProcessContext.h"
#include "FlowFileRecord.h"
#include "Exception.h"
#include "core/logging/LoggerFactory.h"
#include "core/Deprecated.h"
#include "FlowFile.h"
#include "WeakReference.h"
#include "provenance/Provenance.h"
#include "utils/gsl.h"

namespace org::apache::nifi::minifi::core {
namespace detail {
struct ReadBufferResult {
  int64_t status;
  std::vector<std::byte> buffer;
};

std::string to_string(const ReadBufferResult& read_buffer_result);
}  // namespace detail

// ProcessSession Class
class ProcessSession : public ReferenceContainer {
 public:
  // Constructor
  /*!
   * Create a new process session
   */
  explicit ProcessSession(std::shared_ptr<ProcessContext> processContext = nullptr);

  // Destructor
  virtual ~ProcessSession();

  // Commit the session
  void commit();
  // Roll Back the session
  void rollback();
  // Get Provenance Report
  std::shared_ptr<provenance::ProvenanceReporter> getProvenanceReporter() {
    return provenance_report_;
  }
  // writes the created contents to the underlying repository
  void flushContent();

  // Get the FlowFile from the highest priority queue
  virtual std::shared_ptr<core::FlowFile> get();
  // Create a new UUID FlowFile with no content resource claim and inherit all attributes from parent
  std::shared_ptr<core::FlowFile> create(const std::shared_ptr<core::FlowFile> &parent = {});
  // Add a FlowFile to the session
  virtual void add(const std::shared_ptr<core::FlowFile> &record);
  // Clone a new UUID FlowFile from parent both for content resource claim and attributes
  std::shared_ptr<core::FlowFile> clone(const std::shared_ptr<core::FlowFile> &parent);
  // Clone a new UUID FlowFile from parent for attributes and sub set of parent content resource claim
  std::shared_ptr<core::FlowFile> clone(const std::shared_ptr<core::FlowFile> &parent, int64_t offset, int64_t size);
  // Transfer the FlowFile to the relationship
  virtual void transfer(const std::shared_ptr<core::FlowFile> &flow, const Relationship& relationship);
  // Put Attribute
  void putAttribute(const std::shared_ptr<core::FlowFile> &flow, const std::string& key, const std::string& value);
  // Remove Attribute
  void removeAttribute(const std::shared_ptr<core::FlowFile> &flow, const std::string& key);
  // Remove Flow File
  void remove(const std::shared_ptr<core::FlowFile> &flow);
  // Access the contents of the flow file as an input stream; returns null if the flow file has no content claim
  std::shared_ptr<io::InputStream> getFlowFileContentStream(const std::shared_ptr<core::FlowFile>& flow_file);
  // Execute the given read callback against the content
  int64_t read(const std::shared_ptr<core::FlowFile> &flow, const io::InputStreamCallback& callback);
  // Read content into buffer
  detail::ReadBufferResult readBuffer(const std::shared_ptr<core::FlowFile>& flow);
  // Execute the given write callback against the content
  void write(const std::shared_ptr<core::FlowFile> &flow, const io::OutputStreamCallback& callback);
  // Read and write the flow file at the same time (eg. for processing it line by line)
  int64_t readWrite(const std::shared_ptr<core::FlowFile> &flow, const io::InputOutputStreamCallback& callback);
  // Replace content with buffer
  void writeBuffer(const std::shared_ptr<core::FlowFile>& flow_file, gsl::span<const char> buffer);
  void writeBuffer(const std::shared_ptr<core::FlowFile>& flow_file, gsl::span<const std::byte> buffer);
  // Execute the given write/append callback against the content
  void append(const std::shared_ptr<core::FlowFile> &flow, const io::OutputStreamCallback& callback);
  // Append buffer to content
  void appendBuffer(const std::shared_ptr<core::FlowFile>& flow, gsl::span<const char> buffer);
  void appendBuffer(const std::shared_ptr<core::FlowFile>& flow, gsl::span<const std::byte> buffer);
  // Penalize the flow
  void penalize(const std::shared_ptr<core::FlowFile> &flow);

  bool outgoingConnectionsFull(const std::string& relationship);

  /**
   * Imports a file from the data stream
   * @param stream incoming data stream that contains the data to store into a file
   * @param flow flow file
   */
  void importFrom(io::InputStream &stream, const std::shared_ptr<core::FlowFile> &flow);
  void importFrom(io::InputStream&& stream, const std::shared_ptr<core::FlowFile> &flow);

  // import from the data source.
  void import(std::string source, const std::shared_ptr<core::FlowFile> &flow, bool keepSource = true, uint64_t offset = 0);
  DEPRECATED(/*deprecated in*/ 0.7.0, /*will remove in */ 2.0) void import(std::string source, std::vector<std::shared_ptr<FlowFile>> &flows, bool keepSource, uint64_t offset, char inputDelimiter); // NOLINT
  DEPRECATED(/*deprecated in*/ 0.8.0, /*will remove in */ 2.0) void import(const std::string& source, std::vector<std::shared_ptr<FlowFile>> &flows, uint64_t offset, char inputDelimiter);

  /**
   * Exports the data stream to a file
   * @param string file to export stream to
   * @param flow flow file
   * @param bool whether or not to keep the content in the flow file
   */
  bool exportContent(const std::string &destination, const std::shared_ptr<core::FlowFile> &flow,
  bool keepContent);

  bool exportContent(const std::string &destination, const std::string &tmpFileName, const std::shared_ptr<core::FlowFile> &flow,
  bool keepContent);

  // Stash the content to a key
  void stash(const std::string &key, const std::shared_ptr<core::FlowFile> &flow);
  // Restore content previously stashed to a key
  void restore(const std::string &key, const std::shared_ptr<core::FlowFile> &flow);

  bool existsFlowFileInRelationship(const Relationship &relationship);

// Prevent default copy constructor and assignment operation
// Only support pass by reference or pointer
  ProcessSession(const ProcessSession &parent) = delete;
  ProcessSession &operator=(const ProcessSession &parent) = delete;

 protected:
  struct FlowFileUpdate {
    std::shared_ptr<FlowFile> modified;
    std::shared_ptr<FlowFile> snapshot;
  };

  // FlowFiles being modified by current process session
  std::map<utils::Identifier, FlowFileUpdate> _updatedFlowFiles;
  // FlowFiles being added by current process session
  std::map<utils::Identifier, std::shared_ptr<core::FlowFile>> _addedFlowFiles;
  // FlowFiles being deleted by current process session
  std::vector<std::shared_ptr<core::FlowFile>> _deletedFlowFiles;
  // FlowFiles being transfered to the relationship
  std::map<utils::Identifier, Relationship> _transferRelationship;
  // FlowFiles being cloned for multiple connections per relationship
  std::vector<std::shared_ptr<core::FlowFile>> _clonedFlowFiles;

 private:
  enum class RouteResult {
    Ok_Routed,
    Ok_AutoTerminated,
    Ok_Deleted,
    Error_NoRelationship
  };

  RouteResult routeFlowFile(const std::shared_ptr<FlowFile>& record);

  void persistFlowFilesBeforeTransfer(
      std::map<Connectable*, std::vector<std::shared_ptr<core::FlowFile>>>& transactionMap,
      const std::map<utils::Identifier, FlowFileUpdate>& modifiedFlowFiles);

  void ensureNonNullResourceClaim(
      const std::map<Connectable*, std::vector<std::shared_ptr<core::FlowFile>>>& transactionMap);

  // Clone the flow file during transfer to multiple connections for a relationship
  std::shared_ptr<core::FlowFile> cloneDuringTransfer(const std::shared_ptr<core::FlowFile> &parent);
  // ProcessContext
  std::shared_ptr<ProcessContext> process_context_;
  // Logger
  std::shared_ptr<logging::Logger> logger_;
  // Provenance Report
  std::shared_ptr<provenance::ProvenanceReporter> provenance_report_;

  std::shared_ptr<ContentSession> content_session_;

  CoreComponentStateManager* stateManager_;

  static std::shared_ptr<utils::IdGenerator> id_generator_;
};

}  // namespace org::apache::nifi::minifi::core
