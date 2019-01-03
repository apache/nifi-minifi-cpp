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
#ifndef __PROCESS_SESSION_H__
#define __PROCESS_SESSION_H__

#include <uuid/uuid.h>
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
#include "core/logging/LoggerConfiguration.h"
#include "FlowFile.h"
#include "WeakReference.h"
#include "provenance/Provenance.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace core {

// ProcessSession Class
class ProcessSession : public ReferenceContainer {
 public:
  // Constructor
  /*!
   * Create a new process session
   */
  ProcessSession(std::shared_ptr<ProcessContext> processContext = nullptr)
      : process_context_(processContext),
        logger_(logging::LoggerFactory<ProcessSession>::getLogger()) {
    logger_->log_trace("ProcessSession created for %s", process_context_->getProcessorNode()->getName());
    auto repo = processContext->getProvenanceRepository();
    //provenance_report_ = new provenance::ProvenanceReporter(repo, process_context_->getProcessorNode()->getName(), process_context_->getProcessorNode()->getName());
    provenance_report_ = std::make_shared<provenance::ProvenanceReporter>(repo, process_context_->getProcessorNode()->getName(), process_context_->getProcessorNode()->getName());
  }

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
  //
  // Get the FlowFile from the highest priority queue
  virtual std::shared_ptr<core::FlowFile> get();
  // Create a new UUID FlowFile with no content resource claim and without parent
  std::shared_ptr<core::FlowFile> create();
  // Create a new UUID FlowFile with no content resource claim and inherit all attributes from parent
  //std::shared_ptr<core::FlowFile> create(std::shared_ptr<core::FlowFile> &&parent);
  // Create a new UUID FlowFile with no content resource claim and inherit all attributes from parent
  std::shared_ptr<core::FlowFile> create(const std::shared_ptr<core::FlowFile> &parent);
  // Add a FlowFile to the session
  virtual void add(const std::shared_ptr<core::FlowFile> &flow);
// Clone a new UUID FlowFile from parent both for content resource claim and attributes
  std::shared_ptr<core::FlowFile> clone(const std::shared_ptr<core::FlowFile> &parent);
  // Clone a new UUID FlowFile from parent for attributes and sub set of parent content resource claim
  std::shared_ptr<core::FlowFile> clone(const std::shared_ptr<core::FlowFile> &parent, int64_t offset, int64_t size);
  // Duplicate a FlowFile with the same UUID and all attributes and content resource claim for the roll back of the session
  std::shared_ptr<core::FlowFile> duplicate(const std::shared_ptr<core::FlowFile> &original);
  // Transfer the FlowFile to the relationship
  virtual void transfer(const std::shared_ptr<core::FlowFile> &flow, Relationship relationship);
  // Put Attribute
  void putAttribute(const std::shared_ptr<core::FlowFile> &flow, std::string key, std::string value);
  // Remove Attribute
  void removeAttribute(const std::shared_ptr<core::FlowFile> &flow, std::string key);
  // Remove Flow File
  void remove(const std::shared_ptr<core::FlowFile> &flow);
  // Execute the given read callback against the content
  void read(const std::shared_ptr<core::FlowFile> &flow, InputStreamCallback *callback);
  // Execute the given write callback against the content
  void write(const std::shared_ptr<core::FlowFile> &flow, OutputStreamCallback *callback);
  // Execute the given write/append callback against the content
  void append(const std::shared_ptr<core::FlowFile> &flow, OutputStreamCallback *callback);
  // Penalize the flow
  void penalize(const std::shared_ptr<core::FlowFile> &flow);

  /**
   * Imports a file from the data stream
   * @param stream incoming data stream that contains the data to store into a file
   * @param flow flow file
   */
  void importFrom(io::DataStream &stream, const std::shared_ptr<core::FlowFile> &flow);
  // import from the data source.
  void import(std::string source, const std::shared_ptr<core::FlowFile> &flow, bool keepSource = true, uint64_t offset = 0);
  void import(std::string source, std::vector<std::shared_ptr<FlowFileRecord>> &flows, bool keepSource, uint64_t offset, char inputDelimiter);

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

// Prevent default copy constructor and assignment operation
// Only support pass by reference or pointer
  ProcessSession(const ProcessSession &parent) = delete;
  ProcessSession &operator=(const ProcessSession &parent) = delete;

 protected:
// FlowFiles being modified by current process session
  std::map<std::string, std::shared_ptr<core::FlowFile> > _updatedFlowFiles;
  // Copy of the original FlowFiles being modified by current process session as above
  std::map<std::string, std::shared_ptr<core::FlowFile> > _originalFlowFiles;
  // FlowFiles being added by current process session
  std::map<std::string, std::shared_ptr<core::FlowFile> > _addedFlowFiles;
  // FlowFiles being deleted by current process session
  std::map<std::string, std::shared_ptr<core::FlowFile> > _deletedFlowFiles;
  // FlowFiles being transfered to the relationship
  std::map<std::string, Relationship> _transferRelationship;
  // FlowFiles being cloned for multiple connections per relationship
  std::map<std::string, std::shared_ptr<core::FlowFile> > _clonedFlowFiles;

 private:
// Clone the flow file during transfer to multiple connections for a relationship
  std::shared_ptr<core::FlowFile> cloneDuringTransfer(std::shared_ptr<core::FlowFile> &parent);
  // ProcessContext
  std::shared_ptr<ProcessContext> process_context_;
  // Logger
  std::shared_ptr<logging::Logger> logger_;
  // Provenance Report
  std::shared_ptr<provenance::ProvenanceReporter> provenance_report_;

  static std::shared_ptr<utils::IdGenerator> id_generator_;
};

} /* namespace core */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
#endif
