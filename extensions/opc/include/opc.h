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


#pragma once

#include <array>
#include <string>
#include <functional>
#include <map>
#include <vector>
#include <memory>
#include <string_view>
#include <utility>
#include <optional>

#include "open62541/client.h"
#include "minifi-cpp/core/logging/Logger.h"
#include "minifi-cpp/Exception.h"

namespace org::apache::nifi::minifi::opc {

class OPCException : public minifi::Exception {
 public:
  OPCException(ExceptionType type, std::string &&error_msg)
    : Exception(type, error_msg) {
  }
};

enum class OPCNodeIDType{
  Path,
  Int,
  String
};

enum class OPCNodeDataType{
  Int64,
  UInt64,
  Int32,
  UInt32,
  Boolean,
  Float,
  Double,
  String
};

struct NodeData;

class Client;

using NodeFoundCallBackFunc = bool(const UA_ReferenceDescription*, const std::string&);

class Client {
 public:
  ~Client();
  bool isConnected();
  UA_StatusCode connect(const std::string& url, const std::string& username = "", const std::string& password = "");
  NodeData getNodeData(const UA_ReferenceDescription *ref, const std::string& base_path = "");
  UA_ReferenceDescription * getNodeReference(UA_NodeId node_id);
  void traverse(UA_NodeId node_id, const std::function<NodeFoundCallBackFunc>& cb, const std::string& base_path = "", uint64_t max_depth = 0, bool fetch_root = true);
  bool exists(UA_NodeId node_id);
  UA_StatusCode translateBrowsePathsToNodeIdsRequest(const std::string& path, std::vector<UA_NodeId>& found_node_ids, int32_t namespace_index,
    const std::vector<UA_UInt32>& path_reference_types, const std::shared_ptr<core::logging::Logger>& logger);

  template<typename T>
  UA_StatusCode update_node(const UA_NodeId node_id, T value);

  template<typename T>
  UA_StatusCode add_node(const UA_NodeId parent_node_id, const UA_NodeId target_node_id, const UA_UInt32 ref_type_id, std::string_view browse_name, T value, UA_NodeId *received_node_id);

  static std::unique_ptr<Client> createClient(const std::shared_ptr<core::logging::Logger>& logger, const std::string& application_uri,
                                              const std::vector<char>& cert_buffer, const std::vector<char>& key_buffer,
                                              const std::vector<std::vector<char>>& trust_buffers);

 private:
  Client(const std::shared_ptr<core::logging::Logger>& logger, const std::string& application_uri,
      const std::vector<char>& cert_buffer, const std::vector<char>& key_buffer,
      const std::vector<std::vector<char>>& trust_buffers);

  UA_Client *client_;
  std::shared_ptr<core::logging::Logger> logger_;
  UA_Logger minifi_ua_logger_{};
};

using ClientPtr = std::unique_ptr<Client>;

struct NodeData {
  std::vector<uint8_t> data;
  UA_DataTypeKind data_type_id;
  std::map<std::string, std::string> attributes;

  virtual ~NodeData() {
    if (var_) {
      UA_Variant_delete(var_);
    }
  }

  NodeData (const NodeData&) = delete;
  NodeData& operator= (const NodeData &) = delete;
  NodeData& operator= (NodeData &&) = delete;

  NodeData(NodeData&& rhs) : data(rhs.data), attributes(rhs.attributes) {
    data_type_id = rhs.data_type_id;
    this->var_ = rhs.var_;
    rhs.var_ = nullptr;
  }

 private:
  UA_Variant* var_;

  explicit NodeData(UA_Variant * var = nullptr) {
    var_ = var;  // NOLINT(clang-analyzer-optin.cplusplus.UninitializedObject)
  }
  void addVariant(UA_Variant * var) {
    if (var_) {
      UA_Variant_delete(var_);
    }
    var_ = var;
  }

  friend class Client;
  friend std::string nodeValue2String(const NodeData&);
};

std::string nodeValue2String(const NodeData& nd);

std::string OPCDateTime2String(UA_DateTime raw_date);

void logFunc(void *context, UA_LogLevel level, UA_LogCategory category, const char *msg, va_list args);

std::optional<UA_UInt32> mapOpcReferenceType(const std::string& ref_type);

}  // namespace org::apache::nifi::minifi::opc

