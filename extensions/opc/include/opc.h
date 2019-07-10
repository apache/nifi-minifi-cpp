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

#include <string>
#include <functional>

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace opc {

void disconnect(UA_Client *client);

using ClientPtr = std::unique_ptr<UA_Client, decltype(&opc::disconnect)>;

enum class OPCNodeIDType{ Path, Int, String };

enum class OPCNodeDataType{ Int64, UInt64, Int32, UInt32, Boolean, Float, Double, String };

struct nodeData {
  std::vector<uint8_t> data;
  uint16_t dataTypeID;
  std::map<std::string, std::string> attributes;

  virtual ~nodeData(){
    if(var_) {
      UA_Variant_delete(var_);
    }
  }

  nodeData (const nodeData&) = delete;
  nodeData& operator= (const nodeData &) = delete;
  nodeData& operator= (nodeData &&) = delete;

  nodeData(nodeData&& rhs) : data(rhs.data), attributes(rhs.attributes)
  {
    dataTypeID = rhs.dataTypeID;
    this->var_ = rhs.var_;
    rhs.var_ = nullptr;
  }

private:
  UA_Variant* var_;

  nodeData(UA_Variant * var = nullptr) {
    var_ = var;
  }
  void addVariant(UA_Variant * var) {
    if(var_) {
      UA_Variant_delete(var_);
    }
    var_ = var;
  }

  friend nodeData getNodeData(opc::ClientPtr&, const UA_ReferenceDescription*, const std::string&);
  friend std::string nodeValue2String(const nodeData&);
};

static std::map<std::string, OPCNodeDataType>  StringToOPCDataTypeMap = {{"Int64", OPCNodeDataType::Int64}, {"UInt64", OPCNodeDataType::UInt64 }, {"Int32", OPCNodeDataType::Int32},
                                                                         {"UInt32", OPCNodeDataType::UInt32}, {"Boolean", OPCNodeDataType::Boolean}, {"Float", OPCNodeDataType::Float},
                                                                         {"Double", OPCNodeDataType::Double}, {"String", OPCNodeDataType::String}};

int32_t OPCNodeDataTypeToTypeID(OPCNodeDataType dt);

void setCertificates(ClientPtr& clientPtr, const std::vector<char>& certBuffer, const std::vector<char>& keyBuffer);

ClientPtr connect(const std::string& url, const std::shared_ptr<core::logging::Logger>& logger,
    const std::string& username = "", const std::string& password = "");

bool isConnected(const ClientPtr &ptr);

using nodeFoundCallBackFunc = bool(ClientPtr& clientPtr, const UA_ReferenceDescription*, const std::string&);

UA_StatusCode translateBrowsePathsToNodeIdsRequest(ClientPtr& clientPtr, const std::string& path, std::vector<UA_NodeId>& foundNodeIDs, const std::shared_ptr<core::logging::Logger>& logger);

void traverse(ClientPtr& clientPtr, UA_NodeId nodeId, std::function<nodeFoundCallBackFunc> cb, const std::string& basePath = "");

bool exists(ClientPtr& clientPtr, UA_NodeId nodeId);

nodeData getNodeData(opc::ClientPtr& clientPtr, const UA_ReferenceDescription *ref, const std::string& basePath = "");

std::string nodeValue2String(const nodeData& nd);

std::string OPCDateTime2String(UA_DateTime raw_date);

template <typename T>
UA_StatusCode update_node(ClientPtr& clientPtr, const UA_NodeId nodeId, T value);

template <typename T>
UA_StatusCode add_node(ClientPtr& clientPtr, const UA_NodeId parentNodeId, const UA_NodeId targetNodeId, std::string browseName, T value, OPCNodeDataType dt, UA_NodeId *receivedNodeId);

} /* namespace opc */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */


#endif //NIFI_MINIFI_CPP_OPC_H
