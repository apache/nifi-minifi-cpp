/**
 * PutOPC class declaration
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

#ifndef NIFI_MINIFI_CPP_PUTOPC_H
#define NIFI_MINIFI_CPP_PUTOPC_H

#include <memory>
#include <string>
#include <list>
#include <map>
#include <mutex>
#include <thread>

#include "opc.h"
#include "opcbase.h"
#include "utils/ByteArrayCallback.h"
#include "FlowFileRecord.h"
#include "core/Processor.h"
#include "core/ProcessSession.h"
#include "core/Core.h"
#include "core/Property.h"
#include "core/Resource.h"
#include "controllers/SSLContextService.h"
#include "core/logging/LoggerConfiguration.h"
#include "utils/Id.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace processors {

class PutOPCProcessor : public BaseOPCProcessor {
 public:
  static constexpr char const* ProcessorName = "PutOPC";
  // Supported Properties
  //static core::Property OPCServerEndPoint;
  static core::Property ParentNodeIDType;
  static core::Property ParentNodeID;
  static core::Property ParentNameSpaceIndex;
  static core::Property ValueType;

  static core::Property TargetNodeIDType;
  static core::Property TargetNodeID;
  static core::Property TargetNodeBrowseName;
  static core::Property TargetNodeNameSpaceIndex;

  /*static core::Property Username;
  static core::Property Password;
  static core::Property CertificatePath;
  static core::Property KeyPath; */

  // Supported Relationships
  static core::Relationship Success;
  static core::Relationship Failure;

  PutOPCProcessor(std::string name, utils::Identifier uuid = utils::Identifier())
  : BaseOPCProcessor(logging::LoggerFactory<PutOPCProcessor>::getLogger(), name, uuid) {}

  virtual void onSchedule(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSessionFactory> &factory) override;

  virtual void onTrigger(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSession> &session) override;

  virtual void initialize(void) override;


 private:

  class ReadCallback : public InputStreamCallback {
  public:
    ReadCallback(std::shared_ptr<logging::Logger> logger) : logger_(logger) {}
    int64_t process(std::shared_ptr<io::BaseStream> stream) override;
    const std::vector<uint8_t>& getContent() const { return buf_; }

  private:
    std::vector<uint8_t> buf_;
    std::shared_ptr<logging::Logger> logger_;
  };


  // Logger
  //std::shared_ptr<logging::Logger> logger_;
  std::mutex onTriggerMutex_;

  //std::string endPointURL_;
  std::string nodeID_;
  int32_t nameSpaceIdx_;
  opc::OPCNodeIDType idType_;
  UA_NodeId parentNodeID_;

  /*opc::ClientPtr connection_;



  std::string username_;
  std::string password_;
  std::string certpath_;
  std::string keypath_;

  std::vector<char> certBuffer_;
  std::vector<char> keyBuffer_;

  bool configOK_; */
  bool parentExists_;

  opc::OPCNodeDataType nodeDataType_;
};

REGISTER_RESOURCE(PutOPCProcessor, "Creates/updates  OPC nodes");

} /* namespace processors */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

#endif  // NIFI_MINIFI_CPP_PUTOPC_H