/**
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


#ifndef NIFI_MINIFI_CPP_OPC_H
#define NIFI_MINIFI_CPP_OPC_H

#include "open62541/client.h"
#include "open62541/client_highlevel.h"
#include "open62541/client_config_default.h"
#include "logging/Logger.h"
#include "Exception.h"

#include <string>
#include <functional>

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace opc {

class OPCException : public minifi::Exception {
 public:
  OPCException(ExceptionType type, std::string &&errorMsg)
  : Exception(type, errorMsg) {
  }
};

enum class OPCNodeIDType{ Path, Int, String };

enum class OPCNodeDataType{ Int64, UInt64, Int32, UInt32, Boolean, Float, Double, String };

struct NodeData;

class Client;

using nodeFoundCallBackFunc = bool(Client& client, const UA_ReferenceDescription*, const std::string&);

class Client {
 public:
  bool isConnected();
  UA_StatusCode connect(const std::string& url, const std::string& username = "", const std::string& password = "");
  ~Client();
  NodeData getNodeData(const UA_ReferenceDescription *ref, const std::string& basePath = "");
  UA_ReferenceDescription * getNodeReference(UA_NodeId nodeId);
  void traverse(UA_NodeId nodeId, std::function<nodeFoundCallBackFunc> cb, const std::string& basePath = "", uint64_t maxDepth = 0, bool fetchRoot = true);
  bool exists(UA_NodeId nodeId);
  UA_StatusCode translateBrowsePathsToNodeIdsRequest(const std::string& path, std::vector<UA_NodeId>& foundNodeIDs, const std::shared_ptr<core::logging::Logger>& logger);

  template<typename T>
  UA_StatusCode update_node(const UA_NodeId nodeId, T value);

  template<typename T>
  UA_StatusCode add_node(const UA_NodeId parentNodeId, const UA_NodeId targetNodeId, std::string browseName, T value, OPCNodeDataType dt, UA_NodeId *receivedNodeId);

  static std::unique_ptr<Client> createClient(std::shared_ptr<core::logging::Logger> logger, const std::string& applicationURI,
                                              const std::vector<char>& certBuffer, const std::vector<char>& keyBuffer,
                                              const std::vector<std::vector<char>>& trustBuffers);

 private:
  Client (std::shared_ptr<core::logging::Logger> logger, const std::string& applicationURI,
      const std::vector<char>& certBuffer, const std::vector<char>& keyBuffer,
      const std::vector<std::vector<char>>& trustBuffers);

  UA_Client *client_;
  std::shared_ptr<core::logging::Logger> logger_;
};

using ClientPtr = std::unique_ptr<Client>;

struct NodeData {
  std::vector<uint8_t> data;
  uint16_t dataTypeID;
  std::map<std::string, std::string> attributes;

  virtual ~NodeData(){
    if(var_) {
      UA_Variant_delete(var_);
    }
  }

  NodeData (const NodeData&) = delete;
  NodeData& operator= (const NodeData &) = delete;
  NodeData& operator= (NodeData &&) = delete;

  NodeData(NodeData&& rhs) : data(rhs.data), attributes(rhs.attributes)
  {
    dataTypeID = rhs.dataTypeID;
    this->var_ = rhs.var_;
    rhs.var_ = nullptr;
  }

 private:
  UA_Variant* var_;

  NodeData(UA_Variant * var = nullptr) {
    var_ = var;
  }
  void addVariant(UA_Variant * var) {
    if(var_) {
      UA_Variant_delete(var_);
    }
    var_ = var;
  }

  friend class Client;
  friend std::string nodeValue2String(const NodeData&);
};

static std::map<std::string, OPCNodeDataType>  StringToOPCDataTypeMap = {{"Int64", OPCNodeDataType::Int64}, {"UInt64", OPCNodeDataType::UInt64 }, {"Int32", OPCNodeDataType::Int32},
                                                                         {"UInt32", OPCNodeDataType::UInt32}, {"Boolean", OPCNodeDataType::Boolean}, {"Float", OPCNodeDataType::Float},
                                                                         {"Double", OPCNodeDataType::Double}, {"String", OPCNodeDataType::String}};

int32_t OPCNodeDataTypeToTypeID(OPCNodeDataType dt);

std::string nodeValue2String(const NodeData& nd);

std::string OPCDateTime2String(UA_DateTime raw_date);

void logFunc(void *context, UA_LogLevel level, UA_LogCategory category, const char *msg, va_list args);

static void logClear(void *context) {};

} /* namespace opc */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */


#endif //NIFI_MINIFI_CPP_OPC_H
