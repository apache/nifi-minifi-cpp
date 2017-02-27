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
#ifndef __PROCESS_CONTEXT_H__
#define __PROCESS_CONTEXT_H__

#include <uuid/uuid.h>
#include <vector>
#include <queue>
#include <map>
#include <mutex>
#include <atomic>
#include <algorithm>

#include "Property.h"
#include "core/logging/Logger.h"
#include "ProcessorNode.h"
#include "core/Repository.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace core {

// ProcessContext Class
class ProcessContext {
 public:
  // Constructor
  /*!
   * Create a new process context associated with the processor/controller service/state manager
   */
  ProcessContext(ProcessorNode &processor,
                 std::shared_ptr<core::Repository> repo)
      : processor_node_(processor) {
    logger_ = logging::Logger::getLogger();
    repo_ = repo;
  }
  // Destructor
  virtual ~ProcessContext() {
  }
  // Get Processor associated with the Process Context
  ProcessorNode &getProcessorNode() {
    return processor_node_;
  }
  bool getProperty(std::string name, std::string &value) {
    return processor_node_.getProperty(name, value);
  }
  // Sets the property value using the property's string name
  bool setProperty(std::string name, std::string value) {
    return processor_node_.setProperty(name, value);
  }
  // Sets the property value using the Property object
  bool setProperty(Property prop, std::string value) {
    return processor_node_.setProperty(prop, value);
  }
  // Whether the relationship is supported
  bool isSupportedRelationship(Relationship relationship) {
    return processor_node_.isSupportedRelationship(relationship);
  }

  // Check whether the relationship is auto terminated
  bool isAutoTerminated(Relationship relationship) {
    return processor_node_.isAutoTerminated(relationship);
  }
  // Get ProcessContext Maximum Concurrent Tasks
  uint8_t getMaxConcurrentTasks(void) {
    return processor_node_.getMaxConcurrentTasks();
  }
  // Yield based on the yield period
  void yield() {
    processor_node_.yield();
  }

  std::shared_ptr<core::Repository> getProvenanceRepository() {
    return repo_;
  }

  // Prevent default copy constructor and assignment operation
  // Only support pass by reference or pointer
  ProcessContext(const ProcessContext &parent) = delete;
  ProcessContext &operator=(const ProcessContext &parent) = delete;

 private:

  // repository shared pointer.
  std::shared_ptr<core::Repository> repo_;
  // Processor
  ProcessorNode processor_node_;
  // Logger
  std::shared_ptr<logging::Logger> logger_;

};

} /* namespace core */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
#endif
