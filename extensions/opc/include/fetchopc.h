/**
 * FetchOPC class declaration
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
#ifndef NIFI_MINIFI_CPP_FetchOPCProcessor_H
#define NIFI_MINIFI_CPP_FetchOPCProcessor_H

#include <memory>
#include <string>
#include <list>
#include <unordered_map>
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

class FetchOPCProcessor : public BaseOPCProcessor {
public:
  static constexpr char const* ProcessorName = "FetchOPC";
  // Supported Properties
  static core::Property NodeIDType;
  static core::Property NodeID;
  static core::Property NameSpaceIndex;
  static core::Property MaxDepth;
  static core::Property Lazy;

  // Supported Relationships
  static core::Relationship Success;
  static core::Relationship Failure;

  FetchOPCProcessor(std::string name, utils::Identifier uuid = utils::Identifier())
  : BaseOPCProcessor(name, uuid), nameSpaceIdx_(0), nodesFound_(0), variablesFound_(0), maxDepth_(0) {
    logger_ = logging::LoggerFactory<FetchOPCProcessor>::getLogger();
  }

  virtual void onSchedule(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSessionFactory> &factory) override;

  virtual void onTrigger(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSession> &session) override;

  virtual void initialize(void) override;

protected:
  bool nodeFoundCallBack(opc::Client& client, const UA_ReferenceDescription *ref, const std::string& path,
                         const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSession> &session);

  void OPCData2FlowFile(const opc::NodeData& opcnode, const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSession> &session);

  class WriteCallback : public OutputStreamCallback {
    std::string data_;
   public:
    WriteCallback(std::string&& data)
      : data_(data) {
    }
    int64_t process(std::shared_ptr<io::BaseStream> stream) {
      return stream->write(reinterpret_cast<uint8_t*>(const_cast<char*>(data_.c_str())), data_.size());
    }
  };
  std::string nodeID_;
  int32_t nameSpaceIdx_;
  opc::OPCNodeIDType idType_;
  uint32_t nodesFound_;
  uint32_t variablesFound_;
  uint64_t maxDepth_;
  bool lazy_mode_;

private:
  std::mutex onTriggerMutex_;
  std::vector<UA_NodeId> translatedNodeIDs_;  // Only used when user provides path, path->nodeid translation is only done once
  std::unordered_map<std::string, std::string> node_timestamp_; // Key = Full path, Value = Timestamp

};

REGISTER_RESOURCE(FetchOPCProcessor, "Fetches OPC-UA node");

} /* namespace processors */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

#endif //NIFI_MINIFI_CPP_FetchOPCProcessor_H
