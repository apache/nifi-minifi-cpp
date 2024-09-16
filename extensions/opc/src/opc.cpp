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
#include "core/logging/Logger.h"
#include "Exception.h"

#include "utils/gsl.h"

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

Client::Client(const std::shared_ptr<core::logging::Logger>& logger, const std::string& applicationURI,
               const std::vector<char>& certBuffer, const std::vector<char>& keyBuffer,
               const std::vector<std::vector<char>>& trustBuffers)
    : client_(UA_Client_new()) {
  if (certBuffer.empty()) {
    UA_ClientConfig_setDefault(UA_Client_getConfig(client_));
  } else {
    UA_ClientConfig *cc = UA_Client_getConfig(client_);
    cc->securityMode = UA_MESSAGESECURITYMODE_SIGNANDENCRYPT;

    // Certificate
    UA_ByteString certByteString = UA_STRING_NULL;
    certByteString.length = certBuffer.size();
    certByteString.data = reinterpret_cast<UA_Byte*>(UA_malloc(certByteString.length * sizeof(UA_Byte)));  // NOLINT(cppcoreguidelines-owning-memory)
    memcpy(certByteString.data, certBuffer.data(), certByteString.length);

    // Key
    UA_ByteString keyByteString = UA_STRING_NULL;
    keyByteString.length = keyBuffer.size();
    keyByteString.data = reinterpret_cast<UA_Byte*>(UA_malloc(keyByteString.length * sizeof(UA_Byte)));  // NOLINT(cppcoreguidelines-owning-memory)
    memcpy(keyByteString.data, keyBuffer.data(), keyByteString.length);

    // Trusted certificates
    std::vector<UA_ByteString> trustList;
    trustList.resize(trustBuffers.size());
    for (size_t i = 0; i < trustBuffers.size(); i++) {
      trustList[i] = UA_STRING_NULL;
      trustList[i].length = trustBuffers[i].size();
      trustList[i].data = reinterpret_cast<UA_Byte*>(UA_malloc(trustList[i].length * sizeof(UA_Byte)));  // NOLINT(cppcoreguidelines-owning-memory)
      memcpy(trustList[i].data, trustBuffers[i].data(), trustList[i].length);
    }
    UA_StatusCode sc = UA_ClientConfig_setDefaultEncryption(cc, certByteString, keyByteString,
                                                            trustList.data(), trustBuffers.size(),
                                                            nullptr, 0);
    UA_ByteString_clear(&certByteString);
    UA_ByteString_clear(&keyByteString);
    for (size_t i = 0; i < trustBuffers.size(); i++) {
      UA_ByteString_clear(&trustList[i]);
    }
    if (sc != UA_STATUSCODE_GOOD) {
      logger->log_error("Configuring the client for encryption failed: {}", UA_StatusCode_name(sc));
      UA_Client_delete(client_);
      throw OPCException(GENERAL_EXCEPTION, std::string("Failed to created client with the provided encryption settings: ") + UA_StatusCode_name(sc));
    }
  }

  const UA_Logger MinifiUALogger = {logFunc, logger.get(), [](void*){}};

  UA_ClientConfig *configPtr = UA_Client_getConfig(client_);
  configPtr->logger = MinifiUALogger;

  if (applicationURI.length() > 0) {
    UA_String_clear(&configPtr->clientDescription.applicationUri);
    configPtr->clientDescription.applicationUri = UA_STRING_ALLOC(applicationURI.c_str());
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

NodeData Client::getNodeData(const UA_ReferenceDescription *ref, const std::string& basePath) {
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
    nodedata.attributes["Full path"] = basePath + "/" + browsename;
    nodedata.dataTypeID = static_cast<UA_DataTypeKind>(UA_DATATYPEKINDS);
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
      auto server_timestamp = OPCDateTime2String(dv->serverTimestamp);
      auto source_timestamp = OPCDateTime2String(dv->sourceTimestamp);
      nodedata.attributes["Sourcetimestamp"] = source_timestamp;
      UA_ReadResponse_clear(&response);

      nodedata.dataTypeID = static_cast<UA_DataTypeKind>(var->type->typeKind);
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

UA_ReferenceDescription * Client::getNodeReference(UA_NodeId nodeId) {
  UA_ReferenceDescription *ref = UA_ReferenceDescription_new();
  UA_ReferenceDescription_init(ref);
  UA_NodeId_copy(&nodeId, &ref->nodeId.nodeId);
  auto sc = UA_Client_readNodeClassAttribute(client_, nodeId, &ref->nodeClass);
  if (sc == UA_STATUSCODE_GOOD) {
    sc = UA_Client_readBrowseNameAttribute(client_, nodeId, &ref->browseName);
  }
  if (sc == UA_STATUSCODE_GOOD) {
    UA_Client_readDisplayNameAttribute(client_, nodeId, &ref->displayName);
  }
  return ref;
}

void Client::traverse(UA_NodeId nodeId, const std::function<nodeFoundCallBackFunc>& cb, const std::string& basePath, uint64_t maxDepth, bool fetchRoot) {
  if (fetchRoot) {
    UA_ReferenceDescription *rootRef = getNodeReference(nodeId);
    if ((rootRef->nodeClass == UA_NODECLASS_VARIABLE || rootRef->nodeClass == UA_NODECLASS_OBJECT) && rootRef->browseName.name.length > 0) {
      cb(*this, rootRef, basePath);
    }
    UA_ReferenceDescription_delete(rootRef);
  }

  if (maxDepth != 0) {
    maxDepth--;
    if (maxDepth == 0) {
      return;
    }
  }
  UA_BrowseRequest bReq;
  UA_BrowseRequest_init(&bReq);
  bReq.requestedMaxReferencesPerNode = 0;
  bReq.nodesToBrowse = UA_BrowseDescription_new();
  bReq.nodesToBrowseSize = 1;

  UA_NodeId_copy(&nodeId, &bReq.nodesToBrowse[0].nodeId);
  bReq.nodesToBrowse[0].resultMask = UA_BROWSERESULTMASK_ALL;

  UA_BrowseResponse bResp = UA_Client_Service_browse(client_, bReq);

  const auto guard = gsl::finally([&bResp]() {
    UA_BrowseResponse_clear(&bResp);
  });

  UA_BrowseRequest_clear(&bReq);

  for (size_t i = 0; i < bResp.resultsSize; ++i) {
    for (size_t j = 0; j < bResp.results[i].referencesSize; ++j) {
      UA_ReferenceDescription *ref = &(bResp.results[i].references[j]);
      if (cb(*this, ref, basePath)) {
        if (ref->nodeClass == UA_NODECLASS_VARIABLE || ref->nodeClass == UA_NODECLASS_OBJECT) {
          std::string browse_name(reinterpret_cast<char *>(ref->browseName.name.data), ref->browseName.name.length);
          traverse(ref->nodeId.nodeId, cb, basePath + browse_name, maxDepth, false);
        }
      } else {
        return;
      }
    }
  }
}

bool Client::exists(UA_NodeId nodeId) {
  bool retval = false;
  auto callback = [&retval](Client& /*client*/, const UA_ReferenceDescription* /*ref*/, const std::string& /*pat*/) -> bool {
    retval = true;
    return false;  // If any node is found, the given node exists, so traverse can be stopped
  };
  traverse(nodeId, callback, "", 1);
  return retval;
}

UA_StatusCode Client::translateBrowsePathsToNodeIdsRequest(const std::string& path, std::vector<UA_NodeId>& foundNodeIDs, const std::shared_ptr<core::logging::Logger>& logger) {
  logger->log_trace("Trying to find node id for {}", path.c_str());

  auto tokens = utils::string::split(path, "/");
  std::vector<UA_UInt32> ids;
  for (size_t i = 0; i < tokens.size(); ++i) {
    UA_UInt32 val = (i ==0) ? UA_NS0ID_ORGANIZES : UA_NS0ID_HASCOMPONENT;
    ids.push_back(val);
  }

  UA_BrowsePath browsePath;
  UA_BrowsePath_init(&browsePath);
  browsePath.startingNode = UA_NODEID_NUMERIC(0, UA_NS0ID_OBJECTSFOLDER);

  browsePath.relativePath.elements = reinterpret_cast<UA_RelativePathElement*>(UA_Array_new(tokens.size(), &UA_TYPES[UA_TYPES_RELATIVEPATHELEMENT]));
  browsePath.relativePath.elementsSize = tokens.size();

  for (size_t i = 0; i < tokens.size(); ++i) {
    UA_RelativePathElement *elem = &browsePath.relativePath.elements[i];
    elem->referenceTypeId = UA_NODEID_NUMERIC(0, ids[i]);
    elem->targetName = UA_QUALIFIEDNAME_ALLOC(0, tokens[i].c_str());
  }

  UA_TranslateBrowsePathsToNodeIdsRequest request;
  UA_TranslateBrowsePathsToNodeIdsRequest_init(&request);
  request.browsePaths = &browsePath;
  request.browsePathsSize = 1;

  UA_TranslateBrowsePathsToNodeIdsResponse response = UA_Client_Service_translateBrowsePathsToNodeIds(client_, request);

  const auto guard = gsl::finally([&browsePath]() {
    UA_BrowsePath_clear(&browsePath);
  });

  if (response.resultsSize < 1) {
    logger->log_warn("No node id in response for {}", path.c_str());
    return UA_STATUSCODE_BADNODATAAVAILABLE;
  }

  bool foundData = false;

  for (size_t i = 0; i < response.resultsSize; ++i) {
    UA_BrowsePathResult res = response.results[i];
    for (size_t j = 0; j < res.targetsSize; ++j) {
      foundData = true;
      UA_NodeId resultId;
      UA_NodeId_copy(&res.targets[j].targetId.nodeId, &resultId);
      foundNodeIDs.push_back(resultId);
      std::string namespaceUri(reinterpret_cast<char*>(res.targets[j].targetId.namespaceUri.data), res.targets[j].targetId.namespaceUri.length);
    }
  }

  UA_TranslateBrowsePathsToNodeIdsResponse_clear(&response);

  if (foundData) {
    logger->log_debug("Found {} nodes for path {}", foundNodeIDs.size(), path.c_str());
    return UA_STATUSCODE_GOOD;
  } else {
    logger->log_warn("No node id found for path {}", path.c_str());
    return UA_STATUSCODE_BADNODATAAVAILABLE;
  }
}

template<typename T>
UA_StatusCode Client::add_node(const UA_NodeId parentNodeId, const UA_NodeId targetNodeId, std::string_view browseName, T value, UA_NodeId *receivedNodeId) {
  UA_VariableAttributes attr = UA_VariableAttributes_default;
  add_value_to_variant(&attr.value, value);
  char local[6] = "en-US";  // NOLINT(cppcoreguidelines-avoid-c-arrays)
  attr.displayName = UA_LOCALIZEDTEXT(local, const_cast<char*>(browseName.data()));
  UA_StatusCode sc = UA_Client_addVariableNode(client_,
                                               targetNodeId,
                                               parentNodeId,
                                               UA_NODEID_NUMERIC(0, UA_NS0ID_ORGANIZES),
                                               UA_QUALIFIEDNAME(1, const_cast<char*>(browseName.data())),
                                               UA_NODEID_NULL,
                                               attr, receivedNodeId);
  UA_Variant_clear(&attr.value);
  return sc;
}

template<typename T>
UA_StatusCode Client::update_node(const UA_NodeId nodeId, T value) {
  UA_Variant *variant = UA_Variant_new();
  add_value_to_variant(variant, value);
  UA_StatusCode sc = UA_Client_writeValueAttribute(client_, nodeId, variant);
  UA_Variant_delete(variant);
  return sc;
}

std::unique_ptr<Client> Client::createClient(const std::shared_ptr<core::logging::Logger>& logger, const std::string& applicationURI,
                                             const std::vector<char>& certBuffer, const std::vector<char>& keyBuffer,
                                             const std::vector<std::vector<char>>& trustBuffers) {
  try {
    return ClientPtr(new Client(logger, applicationURI, certBuffer, keyBuffer, trustBuffers));
  } catch (const std::exception& exception) {
    logger->log_error("Failed to create client: {}", exception.what());
  }
  return nullptr;
}

template UA_StatusCode Client::update_node<int64_t>(const UA_NodeId nodeId, int64_t value);
template UA_StatusCode Client::update_node<uint64_t>(const UA_NodeId nodeId, uint64_t value);
template UA_StatusCode Client::update_node<int32_t>(const UA_NodeId nodeId, int32_t value);
template UA_StatusCode Client::update_node<uint32_t>(const UA_NodeId nodeId, uint32_t value);
template UA_StatusCode Client::update_node<float>(const UA_NodeId nodeId, float value);
template UA_StatusCode Client::update_node<double>(const UA_NodeId nodeId, double value);
template UA_StatusCode Client::update_node<bool>(const UA_NodeId nodeId, bool value);
template UA_StatusCode Client::update_node<const char *>(const UA_NodeId nodeId, const char * value);
template UA_StatusCode Client::update_node<std::string>(const UA_NodeId nodeId, std::string value);

template UA_StatusCode Client::add_node<int64_t>(const UA_NodeId parentNodeId, const UA_NodeId targetNodeId, std::string_view browseName, int64_t value, UA_NodeId *receivedNodeId);
template UA_StatusCode Client::add_node<uint64_t>(const UA_NodeId parentNodeId, const UA_NodeId targetNodeId, std::string_view browseName, uint64_t value, UA_NodeId *receivedNodeId);
template UA_StatusCode Client::add_node<int32_t>(const UA_NodeId parentNodeId, const UA_NodeId targetNodeId, std::string_view browseName, int32_t value, UA_NodeId *receivedNodeId);
template UA_StatusCode Client::add_node<uint32_t>(const UA_NodeId parentNodeId, const UA_NodeId targetNodeId, std::string_view browseName, uint32_t value, UA_NodeId *receivedNodeId);
template UA_StatusCode Client::add_node<float>(const UA_NodeId parentNodeId, const UA_NodeId targetNodeId, std::string_view browseName, float value, UA_NodeId *receivedNodeId);
template UA_StatusCode Client::add_node<double>(const UA_NodeId parentNodeId, const UA_NodeId targetNodeId, std::string_view browseName, double value, UA_NodeId *receivedNodeId);
template UA_StatusCode Client::add_node<bool>(const UA_NodeId parentNodeId, const UA_NodeId targetNodeId, std::string_view browseName, bool value, UA_NodeId *receivedNodeId);
template UA_StatusCode Client::add_node<const char *>(const UA_NodeId parentNodeId, const UA_NodeId targetNodeId, std::string_view browseName,
    const char * value, UA_NodeId *receivedNodeId);
template UA_StatusCode Client::add_node<std::string>(const UA_NodeId parentNodeId, const UA_NodeId targetNodeId, std::string_view browseName,
    std::string value, UA_NodeId *receivedNodeId);

std::string nodeValue2String(const NodeData& nd) {
  std::string ret_val;
  switch (nd.dataTypeID) {
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

}  // namespace org::apache::nifi::minifi::opc
