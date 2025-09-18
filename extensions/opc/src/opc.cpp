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

#include "opc.h"

#include <cstdlib>
#include <memory>
#include <vector>
#include <string>
#include <functional>
#include <array>

#include "utils/StringUtils.h"
#include "minifi-cpp/core/logging/Logger.h"
#include "minifi-cpp/Exception.h"

#include "minifi-cpp/utils/gsl.h"

#include "open62541/client_highlevel.h"
#include "open62541/client_config_default.h"

namespace org::apache::nifi::minifi::opc {

/*
 * The following functions are only used internally in OPC lib, not to be exported
 */

namespace {

void add_value_to_variant(UA_Variant *variant, std::string &value) {
  UA_String ua_value = UA_STRING(value.data());
  UA_Variant_setScalarCopy(variant, &ua_value, &UA_TYPES[UA_TYPES_STRING]);
}

void add_value_to_variant(UA_Variant *variant, const char *value) {
  std::string strvalue(value);
  add_value_to_variant(variant, strvalue);
}

void add_value_to_variant(UA_Variant *variant, int64_t value) {
  UA_Int64 ua_value = value;
  UA_Variant_setScalarCopy(variant, &ua_value, &UA_TYPES[UA_TYPES_INT64]);
}

void add_value_to_variant(UA_Variant *variant, uint64_t value) {
  UA_UInt64 ua_value = value;
  UA_Variant_setScalarCopy(variant, &ua_value, &UA_TYPES[UA_TYPES_UINT64]);
}

void add_value_to_variant(UA_Variant *variant, int32_t value) {
  UA_Int32 ua_value = value;
  UA_Variant_setScalarCopy(variant, &ua_value, &UA_TYPES[UA_TYPES_INT32]);
}

void add_value_to_variant(UA_Variant *variant, uint32_t value) {
  UA_UInt32 ua_value = value;
  UA_Variant_setScalarCopy(variant, &ua_value, &UA_TYPES[UA_TYPES_UINT32]);
}

void add_value_to_variant(UA_Variant *variant, bool value) {
  UA_Boolean ua_value = value;
  UA_Variant_setScalarCopy(variant, &ua_value, &UA_TYPES[UA_TYPES_BOOLEAN]);
}

void add_value_to_variant(UA_Variant *variant, float value) {
  UA_Float ua_value = value;
  UA_Variant_setScalarCopy(variant, &ua_value, &UA_TYPES[UA_TYPES_FLOAT]);
}

void add_value_to_variant(UA_Variant *variant, double value) {
  UA_Double ua_value = value;
  UA_Variant_setScalarCopy(variant, &ua_value, &UA_TYPES[UA_TYPES_DOUBLE]);
}

core::logging::LOG_LEVEL MapOPCLogLevel(UA_LogLevel ualvl) {
  switch (ualvl) {
    case UA_LOGLEVEL_TRACE:
      return core::logging::trace;
    case UA_LOGLEVEL_DEBUG:
      return core::logging::debug;
    case UA_LOGLEVEL_INFO:
      return core::logging::info;
    case UA_LOGLEVEL_WARNING:
      return core::logging::warn;
    case UA_LOGLEVEL_ERROR:
      return core::logging::err;
    case UA_LOGLEVEL_FATAL:
    default:
      return core::logging::critical;
  }
}
}  // namespace

/*
 * End of internal functions
 */

Client::Client(const std::shared_ptr<core::logging::Logger>& logger, const std::string& application_uri,
               const std::vector<char>& cert_buffer, const std::vector<char>& key_buffer,
               const std::vector<std::vector<char>>& trust_buffers)
    : client_(UA_Client_new()) {
  if (cert_buffer.empty()) {
    UA_ClientConfig_setDefault(UA_Client_getConfig(client_));
  } else {
    UA_ClientConfig *cc = UA_Client_getConfig(client_);
    cc->securityMode = UA_MESSAGESECURITYMODE_SIGNANDENCRYPT;

    // Certificate
    UA_ByteString cert_byte_string = UA_STRING_NULL;
    cert_byte_string.length = cert_buffer.size();
    cert_byte_string.data = reinterpret_cast<UA_Byte*>(UA_malloc(cert_byte_string.length * sizeof(UA_Byte)));  // NOLINT(cppcoreguidelines-owning-memory)
    memcpy(cert_byte_string.data, cert_buffer.data(), cert_byte_string.length);

    // Key
    UA_ByteString key_byte_string = UA_STRING_NULL;
    key_byte_string.length = key_buffer.size();
    key_byte_string.data = reinterpret_cast<UA_Byte*>(UA_malloc(key_byte_string.length * sizeof(UA_Byte)));  // NOLINT(cppcoreguidelines-owning-memory)
    memcpy(key_byte_string.data, key_buffer.data(), key_byte_string.length);

    // Trusted certificates
    std::vector<UA_ByteString> trust_list;
    trust_list.resize(trust_buffers.size());
    for (size_t i = 0; i < trust_buffers.size(); i++) {
      trust_list[i] = UA_STRING_NULL;
      trust_list[i].length = trust_buffers[i].size();
      trust_list[i].data = reinterpret_cast<UA_Byte*>(UA_malloc(trust_list[i].length * sizeof(UA_Byte)));  // NOLINT(cppcoreguidelines-owning-memory)
      memcpy(trust_list[i].data, trust_buffers[i].data(), trust_list[i].length);
    }
    UA_StatusCode sc = UA_ClientConfig_setDefaultEncryption(cc, cert_byte_string, key_byte_string,
                                                            trust_list.data(), trust_buffers.size(),
                                                            nullptr, 0);
    UA_ByteString_clear(&cert_byte_string);
    UA_ByteString_clear(&key_byte_string);
    for (size_t i = 0; i < trust_buffers.size(); i++) {
      UA_ByteString_clear(&trust_list[i]);
    }
    if (sc != UA_STATUSCODE_GOOD) {
      logger->log_error("Configuring the client for encryption failed: {}", UA_StatusCode_name(sc));
      UA_Client_delete(client_);
      throw OPCException(GENERAL_EXCEPTION, std::string("Failed to created client with the provided encryption settings: ") + UA_StatusCode_name(sc));
    }
  }

  minifi_ua_logger_ = {logFunc, logger.get(), [](UA_Logger*){}};

  UA_ClientConfig *config_ptr = UA_Client_getConfig(client_);
  config_ptr->logging = &minifi_ua_logger_;

  if (!application_uri.empty()) {
    UA_String_clear(&config_ptr->clientDescription.applicationUri);
    config_ptr->clientDescription.applicationUri = UA_STRING_ALLOC(application_uri.c_str());
  }

  logger_ = logger;
}

Client::~Client() {
  if (client_ == nullptr) {
    return;
  }

  UA_SecureChannelState channel_state = UA_SECURECHANNELSTATE_CLOSED;
  UA_Client_getState(client_, &channel_state, nullptr, nullptr);
  if (channel_state != UA_SECURECHANNELSTATE_CLOSED) {
    auto sc = UA_Client_disconnect(client_);
    if (sc != UA_STATUSCODE_GOOD) {
      logger_->log_warn("Failed to disconnect OPC client: {}", UA_StatusCode_name(sc));
    }
  }
  UA_Client_delete(client_);
}

bool Client::isConnected() {
  if (!client_) {
    return false;
  }

  UA_SessionState session_state = UA_SESSIONSTATE_CLOSED;
  UA_Client_getState(client_, nullptr, &session_state, nullptr);
  return session_state == UA_SESSIONSTATE_ACTIVATED;
}

UA_StatusCode Client::connect(const std::string& url, const std::string& username, const std::string& password) {
  if (username.empty()) {
    return UA_Client_connect(client_, url.c_str());
  } else {
    return UA_Client_connectUsername(client_, url.c_str(), username.c_str(), password.c_str());
  }
}

NodeData Client::getNodeData(const UA_ReferenceDescription *ref, const std::string& base_path) {
  if (ref->nodeClass == UA_NODECLASS_VARIABLE) {
    opc::NodeData nodedata;
    std::string browsename(reinterpret_cast<const char*>(ref->browseName.name.data), ref->browseName.name.length);

    if (ref->nodeId.nodeId.identifierType == UA_NODEIDTYPE_STRING) {
      std::string nodeidstr(reinterpret_cast<const char*>(ref->nodeId.nodeId.identifier.string.data),  // NOLINT(cppcoreguidelines-pro-type-union-access)
                            ref->nodeId.nodeId.identifier.string.length);  // NOLINT(cppcoreguidelines-pro-type-union-access)
      nodedata.attributes["NodeID"] = nodeidstr;
      nodedata.attributes["NodeID type"] = "string";
    } else if (ref->nodeId.nodeId.identifierType == UA_NODEIDTYPE_BYTESTRING) {
      std::string nodeidstr(reinterpret_cast<const char*>(ref->nodeId.nodeId.identifier.byteString.data),  // NOLINT(cppcoreguidelines-pro-type-union-access)
        ref->nodeId.nodeId.identifier.byteString.length);  // NOLINT(cppcoreguidelines-pro-type-union-access)
      nodedata.attributes["NodeID"] = nodeidstr;
      nodedata.attributes["NodeID type"] = "bytestring";
    } else if (ref->nodeId.nodeId.identifierType == UA_NODEIDTYPE_NUMERIC) {
      nodedata.attributes["NodeID"] = std::to_string(ref->nodeId.nodeId.identifier.numeric);  // NOLINT(cppcoreguidelines-pro-type-union-access)
      nodedata.attributes["NodeID type"] = "numeric";
    }
    nodedata.attributes["Browsename"] = browsename;

    auto splitted_base_path = utils::string::splitAndTrimRemovingEmpty(base_path, "/");
    if (!splitted_base_path.empty() && splitted_base_path.back() == browsename) {
      nodedata.attributes["Full path"] = base_path;
    } else {
      nodedata.attributes["Full path"] = base_path + "/" + browsename;
    }
    nodedata.data_type_id = static_cast<UA_DataTypeKind>(UA_DATATYPEKINDS);
    UA_Variant* var = UA_Variant_new();
    if (UA_Client_readValueAttribute(client_, ref->nodeId.nodeId, var) == UA_STATUSCODE_GOOD && var->type != nullptr && var->data != nullptr) {
      // Because the timestamps are eliminated in readValueAttribute for simplification
      // We need to call the inner function UA_Client_Service_read.
      UA_ReadValueId item;
      UA_ReadValueId_init(&item);
      item.nodeId = ref->nodeId.nodeId;
      item.attributeId = UA_ATTRIBUTEID_VALUE;
      UA_ReadRequest request;
      UA_ReadRequest_init(&request);
      request.nodesToRead = &item;
      request.nodesToReadSize = 1;
      // Differ from ua_client_highlevel.c src
      request.timestampsToReturn = UA_TIMESTAMPSTORETURN_BOTH;
      UA_ReadResponse response = UA_Client_Service_read(client_, request);
      UA_DataValue *dv = response.results;
      auto source_timestamp = OPCDateTime2String(dv->sourceTimestamp);
      nodedata.attributes["Sourcetimestamp"] = source_timestamp;
      UA_ReadResponse_clear(&response);

      nodedata.data_type_id = static_cast<UA_DataTypeKind>(var->type->typeKind);
      nodedata.addVariant(var);
      if (var->type->typeName) {
        nodedata.attributes["Typename"] = std::string(var->type->typeName);
      }
      if (var->type->memSize) {
        nodedata.attributes["Datasize"] = std::to_string(var->type->memSize);
        nodedata.data = std::vector<uint8_t>(var->type->memSize);
        memcpy(nodedata.data.data(), var->data, var->type->memSize);
      }
      return nodedata;
    }
    UA_Variant_delete(var);
    throw OPCException(GENERAL_EXCEPTION, "Failed to read value of node: " + browsename);
  } else {
    throw OPCException(GENERAL_EXCEPTION, "Only variable nodes are supported!");
  }
}

UA_ReferenceDescription * Client::getNodeReference(UA_NodeId node_id) {
  UA_ReferenceDescription *ref = UA_ReferenceDescription_new();
  UA_ReferenceDescription_init(ref);
  UA_NodeId_copy(&node_id, &ref->nodeId.nodeId);
  auto sc = UA_Client_readNodeClassAttribute(client_, node_id, &ref->nodeClass);
  if (sc == UA_STATUSCODE_GOOD) {
    sc = UA_Client_readBrowseNameAttribute(client_, node_id, &ref->browseName);
  }
  if (sc == UA_STATUSCODE_GOOD) {
    UA_Client_readDisplayNameAttribute(client_, node_id, &ref->displayName);
  }
  return ref;
}

void Client::traverse(UA_NodeId node_id, const std::function<NodeFoundCallBackFunc>& cb, const std::string& base_path, uint64_t max_depth, bool fetch_root) {
  if (fetch_root) {
    UA_ReferenceDescription *rootRef = getNodeReference(node_id);
    if ((rootRef->nodeClass == UA_NODECLASS_VARIABLE || rootRef->nodeClass == UA_NODECLASS_OBJECT) && rootRef->browseName.name.length > 0) {
      cb(rootRef, base_path);
    }
    UA_ReferenceDescription_delete(rootRef);
  }

  if (max_depth != 0) {
    max_depth--;
    if (max_depth == 0) {
      return;
    }
  }
  UA_BrowseRequest browse_request;
  UA_BrowseRequest_init(&browse_request);
  browse_request.requestedMaxReferencesPerNode = 0;
  browse_request.nodesToBrowse = UA_BrowseDescription_new();
  browse_request.nodesToBrowseSize = 1;

  UA_NodeId_copy(&node_id, &browse_request.nodesToBrowse[0].nodeId);
  browse_request.nodesToBrowse[0].resultMask = UA_BROWSERESULTMASK_ALL;

  UA_BrowseResponse browse_response = UA_Client_Service_browse(client_, browse_request);

  const auto guard = gsl::finally([&browse_response]() {
    UA_BrowseResponse_clear(&browse_response);
  });

  UA_BrowseRequest_clear(&browse_request);

  for (size_t i = 0; i < browse_response.resultsSize; ++i) {
    for (size_t j = 0; j < browse_response.results[i].referencesSize; ++j) {
      UA_ReferenceDescription *ref = &(browse_response.results[i].references[j]);
      if (cb(ref, base_path)) {
        if (ref->nodeClass == UA_NODECLASS_VARIABLE || ref->nodeClass == UA_NODECLASS_OBJECT) {
          std::string new_base_path = base_path;
          new_base_path.append("/").append(reinterpret_cast<char *>(ref->browseName.name.data), ref->browseName.name.length);
          traverse(ref->nodeId.nodeId, cb, new_base_path, max_depth, false);
        }
      } else {
        return;
      }
    }
  }
}

bool Client::exists(UA_NodeId node_id) {
  bool retval = false;
  auto callback = [&retval](const UA_ReferenceDescription* /*ref*/, const std::string& /*pat*/) -> bool {
    retval = true;
    return false;  // If any node is found, the given node exists, so traverse can be stopped
  };
  traverse(node_id, callback, "", 1);
  return retval;
}

UA_StatusCode Client::translateBrowsePathsToNodeIdsRequest(const std::string& path, std::vector<UA_NodeId>& found_node_ids, int32_t namespace_index,
    const std::vector<UA_UInt32>& path_reference_types, const std::shared_ptr<core::logging::Logger>& logger) {
  logger->log_trace("Trying to find node ids for {}", path.c_str());

  auto tokens = utils::string::splitAndTrimRemovingEmpty(path, "/");
  gsl_Expects(!tokens.empty());

  UA_BrowsePath browse_path;
  UA_BrowsePath_init(&browse_path);
  browse_path.startingNode = UA_NODEID_NUMERIC(0, UA_NS0ID_OBJECTSFOLDER);

  browse_path.relativePath.elements = reinterpret_cast<UA_RelativePathElement*>(UA_Array_new(tokens.size(), &UA_TYPES[UA_TYPES_RELATIVEPATHELEMENT]));
  browse_path.relativePath.elementsSize = tokens.size();

  std::vector<UA_UInt32> ids;
  ids.push_back(UA_NS0ID_ORGANIZES);
  for (size_t i = 0; i < tokens.size() - 1; ++i) {
    if (!path_reference_types.empty()) {
      ids.push_back(path_reference_types[i]);
    } else {
      ids.push_back(UA_NS0ID_ORGANIZES);
    }
  }

  for (size_t i = 0; i < tokens.size(); ++i) {
    UA_RelativePathElement *elem = &browse_path.relativePath.elements[i];
    elem->referenceTypeId = UA_NODEID_NUMERIC(0, ids[i]);
    elem->targetName = UA_QUALIFIEDNAME_ALLOC(namespace_index, tokens[i].c_str());
  }

  UA_TranslateBrowsePathsToNodeIdsRequest request;
  UA_TranslateBrowsePathsToNodeIdsRequest_init(&request);
  request.browsePaths = &browse_path;
  request.browsePathsSize = 1;

  UA_TranslateBrowsePathsToNodeIdsResponse response = UA_Client_Service_translateBrowsePathsToNodeIds(client_, request);

  const auto guard = gsl::finally([&browse_path, &tokens, &response]() {
    for (size_t i = 0; i < tokens.size(); ++i) {
      UA_RelativePathElement *elem = &browse_path.relativePath.elements[i];
      UA_QualifiedName_clear(&elem->targetName);
    }
    UA_BrowsePath_clear(&browse_path);
    UA_TranslateBrowsePathsToNodeIdsResponse_clear(&response);
  });

  if (response.resultsSize < 1 || response.results[0].statusCode != UA_STATUSCODE_GOOD) {
    logger->log_warn("No node id in response for {}", path.c_str());
    return UA_STATUSCODE_BADNODATAAVAILABLE;
  }

  bool found_data = false;

  for (size_t i = 0; i < response.resultsSize; ++i) {
    UA_BrowsePathResult res = response.results[i];
    for (size_t j = 0; j < res.targetsSize; ++j) {
      found_data = true;
      UA_NodeId resultId;
      UA_NodeId_copy(&res.targets[j].targetId.nodeId, &resultId);
      found_node_ids.push_back(resultId);
    }
  }

  if (found_data) {
    logger->log_debug("Found {} nodes for path {}", found_node_ids.size(), path.c_str());
    return UA_STATUSCODE_GOOD;
  } else {
    logger->log_warn("No node id found for path {}", path.c_str());
    return UA_STATUSCODE_BADNODATAAVAILABLE;
  }
}

template<typename T>
UA_StatusCode Client::add_node(const UA_NodeId parent_node_id, const UA_NodeId target_node_id, const UA_UInt32 ref_type_id, std::string_view browse_name, T value, UA_NodeId *received_node_id) {
  UA_VariableAttributes attr = UA_VariableAttributes_default;
  add_value_to_variant(&attr.value, value);
  char local[6] = "en-US";  // NOLINT(cppcoreguidelines-avoid-c-arrays)
  attr.displayName = UA_LOCALIZEDTEXT(local, const_cast<char*>(browse_name.data()));
  UA_StatusCode sc = UA_Client_addVariableNode(client_,
                                               target_node_id,
                                               parent_node_id,
                                               UA_NODEID_NUMERIC(0, ref_type_id),
                                               UA_QUALIFIEDNAME(target_node_id.namespaceIndex, const_cast<char*>(browse_name.data())),
                                               UA_NODEID_NULL,
                                               attr, received_node_id);
  UA_Variant_clear(&attr.value);
  return sc;
}

template<typename T>
UA_StatusCode Client::update_node(const UA_NodeId node_id, T value) {
  UA_Variant *variant = UA_Variant_new();
  add_value_to_variant(variant, value);
  UA_StatusCode sc = UA_Client_writeValueAttribute(client_, node_id, variant);
  UA_Variant_delete(variant);
  return sc;
}

std::unique_ptr<Client> Client::createClient(const std::shared_ptr<core::logging::Logger>& logger, const std::string& application_uri,
                                             const std::vector<char>& cert_buffer, const std::vector<char>& key_buffer,
                                             const std::vector<std::vector<char>>& trust_buffers) {
  try {
    return ClientPtr(new Client(logger, application_uri, cert_buffer, key_buffer, trust_buffers));
  } catch (const std::exception& exception) {
    logger->log_error("Failed to create client: {}", exception.what());
  }
  return nullptr;
}

template UA_StatusCode Client::update_node<int64_t>(const UA_NodeId node_id, int64_t value);
template UA_StatusCode Client::update_node<uint64_t>(const UA_NodeId node_id, uint64_t value);
template UA_StatusCode Client::update_node<int32_t>(const UA_NodeId node_id, int32_t value);
template UA_StatusCode Client::update_node<uint32_t>(const UA_NodeId node_id, uint32_t value);
template UA_StatusCode Client::update_node<float>(const UA_NodeId node_id, float value);
template UA_StatusCode Client::update_node<double>(const UA_NodeId node_id, double value);
template UA_StatusCode Client::update_node<bool>(const UA_NodeId node_id, bool value);
template UA_StatusCode Client::update_node<const char *>(const UA_NodeId node_id, const char * value);
template UA_StatusCode Client::update_node<std::string>(const UA_NodeId node_id, std::string value);

template UA_StatusCode Client::add_node<int64_t>(const UA_NodeId parent_node_id, const UA_NodeId target_node_id, const UA_UInt32 ref_type_id, std::string_view browse_name,
  int64_t value, UA_NodeId *received_node_id);
template UA_StatusCode Client::add_node<uint64_t>(const UA_NodeId parent_node_id, const UA_NodeId target_node_id, const UA_UInt32 ref_type_id, std::string_view browse_name,
  uint64_t value, UA_NodeId *received_node_id);
template UA_StatusCode Client::add_node<int32_t>(const UA_NodeId parent_node_id, const UA_NodeId target_node_id, const UA_UInt32 ref_type_id, std::string_view browse_name,
  int32_t value, UA_NodeId *received_node_id);
template UA_StatusCode Client::add_node<uint32_t>(const UA_NodeId parent_node_id, const UA_NodeId target_node_id, const UA_UInt32 ref_type_id, std::string_view browse_name,
  uint32_t value, UA_NodeId *received_node_id);
template UA_StatusCode Client::add_node<float>(const UA_NodeId parent_node_id, const UA_NodeId target_node_id, const UA_UInt32 ref_type_id, std::string_view browse_name,
  float value, UA_NodeId *received_node_id);
template UA_StatusCode Client::add_node<double>(const UA_NodeId parent_node_id, const UA_NodeId target_node_id, const UA_UInt32 ref_type_id, std::string_view browse_name,
  double value, UA_NodeId *received_node_id);
template UA_StatusCode Client::add_node<bool>(const UA_NodeId parent_node_id, const UA_NodeId target_node_id, const UA_UInt32 ref_type_id, std::string_view browse_name,
  bool value, UA_NodeId *received_node_id);
template UA_StatusCode Client::add_node<const char *>(const UA_NodeId parent_node_id, const UA_NodeId target_node_id, const UA_UInt32 ref_type_id, std::string_view browse_name,
  const char * value, UA_NodeId *received_node_id);
template UA_StatusCode Client::add_node<std::string>(const UA_NodeId parent_node_id, const UA_NodeId target_node_id, const UA_UInt32 ref_type_id, std::string_view browse_name,
  std::string value, UA_NodeId *received_node_id);

std::string nodeValue2String(const NodeData& nd) {
  std::string ret_val;
  switch (nd.data_type_id) {
    case UA_DATATYPEKIND_STRING:
    case UA_DATATYPEKIND_LOCALIZEDTEXT:
    case UA_DATATYPEKIND_BYTESTRING: {
      UA_String value = *reinterpret_cast<UA_String *>(nd.var_->data);
      ret_val = std::string(reinterpret_cast<const char *>(value.data), value.length);
      break;
    }
    case UA_DATATYPEKIND_BOOLEAN: {
      bool b = false;
      memcpy(&b, nd.data.data(), sizeof(bool));
      ret_val = b ? "True" : "False";
      break;
    }
    case UA_DATATYPEKIND_SBYTE: {
      int8_t i8t = 0;
      memcpy(&i8t, nd.data.data(), sizeof(i8t));
      ret_val = std::to_string(i8t);
      break;
    }
    case UA_DATATYPEKIND_BYTE: {
      uint8_t ui8t = 0;
      memcpy(&ui8t, nd.data.data(), sizeof(ui8t));
      ret_val = std::to_string(ui8t);
      break;
    }
    case UA_DATATYPEKIND_INT16: {
      int16_t i16t = 0;
      memcpy(&i16t, nd.data.data(), sizeof(i16t));
      ret_val = std::to_string(i16t);
      break;
    }
    case UA_DATATYPEKIND_UINT16: {
      uint16_t ui16t = 0;
      memcpy(&ui16t, nd.data.data(), sizeof(ui16t));
      ret_val = std::to_string(ui16t);
      break;
    }
    case UA_DATATYPEKIND_INT32: {
      int32_t i32t = 0;
      memcpy(&i32t, nd.data.data(), sizeof(i32t));
      ret_val = std::to_string(i32t);
      break;
    }
    case UA_DATATYPEKIND_UINT32: {
      uint32_t ui32t = 0;
      memcpy(&ui32t, nd.data.data(), sizeof(ui32t));
      ret_val = std::to_string(ui32t);
      break;
    }
    case UA_DATATYPEKIND_INT64: {
      int64_t i64t = 0;
      memcpy(&i64t, nd.data.data(), sizeof(i64t));
      ret_val = std::to_string(i64t);
      break;
    }
    case UA_DATATYPEKIND_UINT64: {
      uint64_t ui64t = 0;
      memcpy(&ui64t, nd.data.data(), sizeof(ui64t));
      ret_val = std::to_string(ui64t);
      break;
    }
    case UA_DATATYPEKIND_FLOAT: {
      if (sizeof(float) == 4 && std::numeric_limits<float>::is_iec559) {
        float f = 0;
        memcpy(&f, nd.data.data(), sizeof(float));
        ret_val = std::to_string(f);
      } else {
        throw OPCException(GENERAL_EXCEPTION, "Float is non-standard on this system, OPC data cannot be extracted!");
      }
      break;
    }
    case UA_DATATYPEKIND_DOUBLE: {
      if (sizeof(double) == 8 && std::numeric_limits<double>::is_iec559) {
        double d = 0;
        memcpy(&d, nd.data.data(), sizeof(double));
        ret_val = std::to_string(d);
      } else {
        throw OPCException(GENERAL_EXCEPTION, "Double is non-standard on this system, OPC data cannot be extracted!");
      }
      break;
    }
    case UA_DATATYPEKIND_DATETIME: {
      UA_DateTime dt = 0;
      memcpy(&dt, nd.data.data(), sizeof(UA_DateTime));
      ret_val = opc::OPCDateTime2String(dt);
      break;
    }
    default:
      throw OPCException(GENERAL_EXCEPTION, "Data type is not supported ");
  }
  return ret_val;
}

std::string OPCDateTime2String(UA_DateTime raw_date) {
  UA_DateTimeStruct dts = UA_DateTime_toStruct(raw_date);
  std::array<char, 100> charBuf{};

  int sz = snprintf(charBuf.data(), charBuf.size(), "%02hu-%02hu-%04hu %02hu:%02hu:%02hu.%03hu", dts.day, dts.month, dts.year, dts.hour, dts.min, dts.sec, dts.milliSec);

  return {charBuf.data(), gsl::narrow<std::size_t>(sz)};
}

void logFunc(void *context, UA_LogLevel level, UA_LogCategory /*category*/, const char *msg, va_list args) {
  std::array<char, 1024> buffer{};
  (void)vsnprintf(buffer.data(), buffer.size(), msg, args);
  auto loggerPtr = reinterpret_cast<core::logging::Logger*>(context);
  loggerPtr->log_string(MapOPCLogLevel(level), buffer.data());
}

std::optional<UA_UInt32> mapOpcReferenceType(const std::string& ref_type) {
  if (ref_type == "Organizes") {
    return UA_NS0ID_ORGANIZES;
  } else if (ref_type == "HasComponent") {
    return UA_NS0ID_HASCOMPONENT;
  } else if (ref_type == "HasProperty") {
    return UA_NS0ID_HASPROPERTY;
  } else if (ref_type == "HasSubtype") {
    return UA_NS0ID_HASSUBTYPE;
  }

  return std::nullopt;
}

}  // namespace org::apache::nifi::minifi::opc
