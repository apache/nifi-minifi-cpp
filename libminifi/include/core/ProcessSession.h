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
#include "provenance/Provenance.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace core {

// ProcessSession Class
class ProcessSession {
 public:
  // Constructor
  /*!
   * Create a new process session
   */
  ProcessSession(ProcessContext *processContext = NULL)
      : process_context_(processContext), logger_(logging::LoggerFactory<ProcessSession>::getLogger()) {
    logger_->log_trace("ProcessSession created for %s",
                       process_context_->getProcessorNode().getName().c_str());
    auto repo = processContext->getProvenanceRepository();
    provenance_report_ = new provenance::ProvenanceReporter(
        repo, process_context_->getProcessorNode().getUUIDStr(),
        process_context_->getProcessorNode().getName());
  }

// Destructor
  virtual ~ProcessSession() {
    if (provenance_report_)
      delete provenance_report_;
  }
// Commit the session
  void commit();
// Roll Back the session
  void rollback();
// Get Provenance Report
  provenance::ProvenanceReporter *getProvenanceReporter() {
    return provenance_report_;
  }
  //
  // Get the FlowFile from the highest priority queue
  std::shared_ptr<core::FlowFile> get();
  // Create a new UUID FlowFile with no content resource claim and without parent
  std::shared_ptr<core::FlowFile> create();
  // Create a new UUID FlowFile with no content resource claim and inherit all attributes from parent
  std::shared_ptr<core::FlowFile> create(
      std::shared_ptr<core::FlowFile> &&parent);
  // Create a new UUID FlowFile with no content resource claim and inherit all attributes from parent
  std::shared_ptr<core::FlowFile> create(
      std::shared_ptr<core::FlowFile> &parent) {
    return create(parent);
  }
// Clone a new UUID FlowFile from parent both for content resource claim and attributes
  std::shared_ptr<core::FlowFile> clone(
      std::shared_ptr<core::FlowFile> &parent);
// Clone a new UUID FlowFile from parent for attributes and sub set of parent content resource claim
  std::shared_ptr<core::FlowFile> clone(std::shared_ptr<core::FlowFile> &parent,
                                        int64_t offset, int64_t size);
// Duplicate a FlowFile with the same UUID and all attributes and content resource claim for the roll back of the session
  std::shared_ptr<core::FlowFile> duplicate(
      std::shared_ptr<core::FlowFile> &original);
// Transfer the FlowFile to the relationship
  void transfer(std::shared_ptr<core::FlowFile> &flow,
                Relationship relationship);
  void transfer(std::shared_ptr<core::FlowFile> &&flow,
                Relationship relationship);
// Put Attribute
  void putAttribute(std::shared_ptr<core::FlowFile> &flow, std::string key,
                    std::string value);
  void putAttribute(std::shared_ptr<core::FlowFile> &&flow, std::string key,
                    std::string value);
// Remove Attribute
  void removeAttribute(std::shared_ptr<core::FlowFile> &flow, std::string key);
  void removeAttribute(std::shared_ptr<core::FlowFile> &&flow, std::string key);
// Remove Flow File
  void remove(std::shared_ptr<core::FlowFile> &flow);
  void remove(std::shared_ptr<core::FlowFile> &&flow);
// Execute the given read callback against the content
  void read(std::shared_ptr<core::FlowFile> &flow,
            InputStreamCallback *callback);
  void read(std::shared_ptr<core::FlowFile> &&flow,
            InputStreamCallback *callback);
// Execute the given write callback against the content
  void write(std::shared_ptr<core::FlowFile> &flow,
             OutputStreamCallback *callback);
  void write(std::shared_ptr<core::FlowFile> &&flow,
             OutputStreamCallback *callback);
// Execute the given write/append callback against the content
  void append(std::shared_ptr<core::FlowFile> &flow,
              OutputStreamCallback *callback);
  void append(std::shared_ptr<core::FlowFile> &&flow,
              OutputStreamCallback *callback);
// Penalize the flow
  void penalize(std::shared_ptr<core::FlowFile> &flow);
  void penalize(std::shared_ptr<core::FlowFile> &&flow);

  /**
   * Imports a file from the data stream
   * @param stream incoming data stream that contains the data to store into a file
   * @param flow flow file
   */
  void importFrom(io::DataStream &stream,
                  std::shared_ptr<core::FlowFile> &&flow);
  // import from the data source.
  void import(std::string source, std::shared_ptr<core::FlowFile> &flow,
              bool keepSource = true, uint64_t offset = 0);
  void import(std::string source, std::shared_ptr<core::FlowFile> &&flow,
              bool keepSource = true, uint64_t offset = 0);

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
  std::shared_ptr<core::FlowFile> cloneDuringTransfer(
      std::shared_ptr<core::FlowFile> &parent);
// ProcessContext
  ProcessContext *process_context_;
// Logger
  std::shared_ptr<logging::Logger> logger_;
// Provenance Report
  provenance::ProvenanceReporter *provenance_report_;

}
;
} /* namespace core */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
#endif
