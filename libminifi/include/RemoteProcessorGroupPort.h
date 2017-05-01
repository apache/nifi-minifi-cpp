/**
 * @file RemoteProcessorGroupPort.h
 * RemoteProcessorGroupPort class declaration
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
#ifndef __REMOTE_PROCESSOR_GROUP_PORT_H__
#define __REMOTE_PROCESSOR_GROUP_PORT_H__

#include <mutex>
#include <memory>
#include <stack>
#include "FlowFileRecord.h"
#include "core/Processor.h"
#include "core/ProcessSession.h"
#include "Site2SiteClientProtocol.h"
#include "io/StreamFactory.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
// RemoteProcessorGroupPort Class
class RemoteProcessorGroupPort : public core::Processor {
 public:
  // Constructor
  /*!
   * Create a new processor
   */
  RemoteProcessorGroupPort(std::shared_ptr<io::StreamFactory> stream_factory, std::string name, uuid_t uuid = NULL)
      : core::Processor(name, uuid),
        direction_(SEND),
        transmitting_(false) {
    stream_factory_ = stream_factory;
    logger_ = logging::Logger::getLogger();
    uuid_copy(protocol_uuid_, uuid);
  }
  // Destructor
  virtual ~RemoteProcessorGroupPort() {

  }
  // Processor Name
  static const std::string ProcessorName;
  // Supported Properties
  static core::Property hostName;
  static core::Property port;
  static core::Property portUUID;
  // Supported Relationships
  static core::Relationship relation;
 public:
  // OnTrigger method, implemented by NiFi RemoteProcessorGroupPort
  virtual void onTrigger(core::ProcessContext *context,
                         core::ProcessSession *session);
  // Initialize, over write by NiFi RemoteProcessorGroupPort
  virtual void initialize(void);
  // Set Direction
  void setDirection(TransferDirection direction) {
    direction_ = direction;
    if (direction_ == RECEIVE)
      this->setTriggerWhenEmpty(true);
  }
  // Set Timeout
  void setTimeOut(uint64_t timeout) {
    timeout_ = timeout;
  }
  // SetTransmitting
  void setTransmitting(bool val) {
    transmitting_ = val;
  }

 protected:

 private:
  std::shared_ptr<io::StreamFactory> stream_factory_;
  std::unique_ptr<Site2SiteClientProtocol> getNextProtocol();
  void returnProtocol(std::unique_ptr<Site2SiteClientProtocol> protocol);

  std::stack<std::unique_ptr<Site2SiteClientProtocol>> available_protocols_;
  std::mutex protocol_mutex_;
  // Logger
  std::shared_ptr<logging::Logger> logger_;
  // Transaction Direction
  TransferDirection direction_;
  // Transmitting
  bool transmitting_;
  // timeout
  uint64_t timeout_;

  uuid_t protocol_uuid_;

};

} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
#endif
