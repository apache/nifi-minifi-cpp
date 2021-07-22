/**
 * PutOPC class definition
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

#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <optional>
#include <thread>

#include "opc.h"
#include "putopc.h"
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
#include "utils/StringUtils.h"
#include "utils/gsl.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace processors {

  core::Property PutOPCProcessor::ParentNodeID(
      core::PropertyBuilder::createProperty("Parent node ID")
          ->withDescription("Specifies the ID of the root node to traverse")
          ->isRequired(true)->build());


  core::Property PutOPCProcessor::ParentNodeIDType(
      core::PropertyBuilder::createProperty("Parent node ID type")
          ->withDescription("Specifies the type of the provided node ID")
          ->isRequired(true)
          ->withAllowableValues<std::string>({"Path", "Int", "String"})->build());

  core::Property PutOPCProcessor::ParentNameSpaceIndex(
      core::PropertyBuilder::createProperty("Parent node namespace index")
          ->withDescription("The index of the namespace. Used only if node ID type is not path.")
          ->withDefaultValue<int32_t>(0)->build());

  core::Property PutOPCProcessor::ValueType(
      core::PropertyBuilder::createProperty("Value type")
          ->withDescription("Set the OPC value type of the created nodes")
          ->isRequired(true)->build());

  core::Property PutOPCProcessor::TargetNodeIDType(
      core::PropertyBuilder::createProperty("Target node ID type")
          ->withDescription("ID type of target node. Allowed values are: Int, String.")
          ->supportsExpressionLanguage(true)->build());

  core::Property PutOPCProcessor::TargetNodeID(
      core::PropertyBuilder::createProperty("Target node ID")
          ->withDescription("ID of target node.")
          ->supportsExpressionLanguage(true)->build());

  core::Property PutOPCProcessor::TargetNodeNameSpaceIndex(
      core::PropertyBuilder::createProperty("Target node namespace index")
          ->withDescription("The index of the namespace. Used only if node ID type is not path.")
          ->supportsExpressionLanguage(true)->build());

  core::Property PutOPCProcessor::TargetNodeBrowseName(
      core::PropertyBuilder::createProperty("Target node browse name")
          ->withDescription("Browse name of target node. Only used when new node is created.")
          ->supportsExpressionLanguage(true)->build());

  static core::Property TargetNodeID;
  static core::Property TargetNodeBrowseName;


  core::Relationship PutOPCProcessor::Success("success", "Successfully put OPC-UA node");
  core::Relationship PutOPCProcessor::Failure("failure", "Failed to put OPC-UA node");

  void PutOPCProcessor::initialize() {
    PutOPCProcessor::ValueType.clearAllowedValues();
    core::PropertyValue pv;
    for (const auto& kv : opc::StringToOPCDataTypeMap) {
      pv = kv.first;
      PutOPCProcessor::ValueType.addAllowedValue(pv);
    }
    std::set<core::Property> putOPCProperties = {ParentNodeID, ParentNodeIDType, ParentNameSpaceIndex, ValueType, TargetNodeIDType, TargetNodeID, TargetNodeNameSpaceIndex, TargetNodeBrowseName};
    std::set<core::Property> baseOPCProperties = BaseOPCProcessor::getSupportedProperties();
    putOPCProperties.insert(baseOPCProperties.begin(), baseOPCProperties.end());
    setSupportedProperties(putOPCProperties);

    // Set the supported relationships
    setSupportedRelationships({Success, Failure});
  }

  void PutOPCProcessor::onSchedule(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSessionFactory> &factory) {
    logger_->log_trace("PutOPCProcessor::onSchedule");

    parentExists_ = false;

    BaseOPCProcessor::onSchedule(context, factory);

    std::string value;
    context->getProperty(ParentNodeID.getName(), nodeID_);
    context->getProperty(ParentNodeIDType.getName(), value);

    if (value == "String") {
      idType_ = opc::OPCNodeIDType::String;
    } else if (value == "Int") {
      idType_ = opc::OPCNodeIDType::Int;
    } else if (value == "Path") {
      idType_ = opc::OPCNodeIDType::Path;
    } else {
      // Where have our validators gone?
      auto error_msg = utils::StringUtils::join_pack(value, " is not a valid node ID type!");
      throw Exception(PROCESS_SCHEDULE_EXCEPTION, error_msg);
    }

    if (idType_ == opc::OPCNodeIDType::Int) {
      try {
        std::stoi(nodeID_);
      } catch(...) {
        auto error_msg = utils::StringUtils::join_pack(nodeID_, " cannot be used as an int type node ID");
        throw Exception(PROCESS_SCHEDULE_EXCEPTION, error_msg);
      }
    }
    if (idType_ != opc::OPCNodeIDType::Path) {
      if (!context->getProperty(ParentNameSpaceIndex.getName(), nameSpaceIdx_)) {
        auto error_msg = utils::StringUtils::join_pack(ParentNameSpaceIndex.getName(), " is mandatory in case ", ParentNodeIDType.getName(), " is not Path");
        throw Exception(PROCESS_SCHEDULE_EXCEPTION, error_msg);
      }
    }

    std::string typestr;
    context->getProperty(ValueType.getName(), typestr);
    nodeDataType_ = opc::StringToOPCDataTypeMap.at(typestr);  // This throws, but allowed values are generated based on this map -> that's a really unexpected error
  }

  void PutOPCProcessor::onTrigger(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSession> &session) {
    logger_->log_trace("PutOPCProcessor::onTrigger");

    std::unique_lock<std::mutex> lock(onTriggerMutex_, std::try_to_lock);
    if (!lock.owns_lock()) {
      logger_->log_warn("processor was triggered before previous listing finished, configuration should be revised!");
      return;
    }

    if (!reconnect()) {
      yield();
      return;
    }

    if (!parentExists_) {
      if (idType_ == opc::OPCNodeIDType::Path) {
        std::vector<UA_NodeId> translatedNodeIDs;
        if (connection_->translateBrowsePathsToNodeIdsRequest(nodeID_, translatedNodeIDs, logger_) !=
            UA_STATUSCODE_GOOD) {
          logger_->log_error("Failed to translate %s to node id, no flow files will be put", nodeID_.c_str());
          yield();
          return;
        } else if (translatedNodeIDs.size() != 1) {
          logger_->log_error("%s was translated to multiple node ids, no flow files will be put", nodeID_.c_str());
          yield();
          return;
        } else {
          parentNodeID_ = translatedNodeIDs[0];
          parentExists_ = true;
        }
      } else {
        parentNodeID_.namespaceIndex = nameSpaceIdx_;
        if (idType_ == opc::OPCNodeIDType::Int) {
          parentNodeID_.identifierType = UA_NODEIDTYPE_NUMERIC;
          parentNodeID_.identifier.numeric = std::stoi(nodeID_);
        } else if (idType_ == opc::OPCNodeIDType::String) {
          parentNodeID_.identifierType = UA_NODEIDTYPE_STRING;
          parentNodeID_.identifier.string = UA_STRING_ALLOC(nodeID_.c_str());
        }
        if (!connection_->exists(parentNodeID_)) {
          logger_->log_error("Parent node doesn't exist, no flow files will be put");
          yield();
          return;
        }
        parentExists_ = true;
      }
    }

    auto flowFile = session->get();

    // Do nothing if there are no incoming files
    if (!flowFile) {
      return;
    }

    std::string targetidtype;

    bool targetNodeExists = false;
    bool targetNodeValid = false;
    UA_NodeId targetnode;

    if (context->getProperty(TargetNodeIDType, targetidtype, flowFile)) {
      std::string targetid;
      std::string namespaceidx;


      if (!context->getProperty(TargetNodeID, targetid, flowFile)) {
        logger_->log_error("Flowfile %s had target node ID type specified (%s) without ID, routing to failure!",
                           flowFile->getUUIDStr(), targetidtype);
        session->transfer(flowFile, Failure);
        return;
      }

      if (!context->getProperty(TargetNodeNameSpaceIndex, namespaceidx, flowFile)) {
        logger_->log_error(
            "Flowfile %s had target node ID type specified (%s) without namespace index, routing to failure!",
            flowFile->getUUIDStr(), targetidtype);
        session->transfer(flowFile, Failure);
        return;
      }
      int32_t nsi;
      try {
        nsi = std::stoi(namespaceidx);
      } catch (...) {
        logger_->log_error("Flowfile %s has invalid namespace index (%s), routing to failure!",
                           flowFile->getUUIDStr(), namespaceidx);
        session->transfer(flowFile, Failure);
        return;
      }

      targetnode.namespaceIndex = nsi;
      if (targetidtype == "Int") {
        targetnode.identifierType = UA_NODEIDTYPE_NUMERIC;
        try {
          targetnode.identifier.numeric = std::stoi(targetid);
          targetNodeValid = true;
        } catch (...) {
          logger_->log_error("Flowfile %s: target node ID is not a valid integer: %s. Routing to failure!",
                             flowFile->getUUIDStr(), targetid);
          session->transfer(flowFile, Failure);
          return;
        }
      } else if (targetidtype == "String") {
        targetnode.identifierType = UA_NODEIDTYPE_STRING;
        targetnode.identifier.string = UA_STRING_ALLOC(targetid.c_str());
        targetNodeValid = true;
      } else {
        logger_->log_error("Flowfile %s: target node ID type is invalid: %s. Routing to failure!",
                           flowFile->getUUIDStr(), targetidtype);
        session->transfer(flowFile, Failure);
        return;
      }
      targetNodeExists = connection_->exists(targetnode);
    }

    ReadCallback cb(logger_);
    session->read(flowFile, &cb);

    const auto &vec = cb.getContent();

    std::string contentstr(reinterpret_cast<const char *>(vec.data()), vec.size());

    if (targetNodeExists) {
      logger_->log_trace("Node exists, trying to update it");
      try {
        UA_StatusCode sc;
        switch (nodeDataType_) {
          case opc::OPCNodeDataType::Int64: {
            int64_t value = std::stoll(contentstr);
            sc = connection_->update_node(targetnode, value);
            break;
          }
          case opc::OPCNodeDataType::UInt64: {
            uint64_t value = std::stoull(contentstr);
            sc = connection_->update_node(targetnode, value);
            break;
          }
          case opc::OPCNodeDataType::Int32: {
            int32_t value = std::stoi(contentstr);
            sc = connection_->update_node(targetnode, value);
            break;
          }
          case opc::OPCNodeDataType::UInt32: {
            uint32_t value = std::stoul(contentstr);
            sc = connection_->update_node(targetnode, value);
            break;
          }
          case opc::OPCNodeDataType::Boolean: {
            const auto contentstr_parsed = utils::StringUtils::toBool(contentstr);
            if (contentstr_parsed) {
              sc = connection_->update_node(targetnode, contentstr_parsed.value());
            } else {
              throw opc::OPCException(GENERAL_EXCEPTION, "Content cannot be converted to bool");
            }
            break;
          }
          case opc::OPCNodeDataType::Float: {
            float value = std::stof(contentstr);
            sc = connection_->update_node(targetnode, value);
            break;
          }
          case opc::OPCNodeDataType::Double: {
            double value = std::stod(contentstr);
            sc = connection_->update_node(targetnode, value);
            break;
          }
          case opc::OPCNodeDataType::String: {
            sc = connection_->update_node(targetnode, contentstr);
            break;
          }
          default:
            throw opc::OPCException(GENERAL_EXCEPTION, "This should never happen!");
        }
        if (sc != UA_STATUSCODE_GOOD) {
          logger_->log_error("Failed to update node: %s", UA_StatusCode_name(sc));
          session->transfer(flowFile, Failure);
          return;
        }
      } catch (...) {
        std::string typestr;
        context->getProperty(ValueType.getName(), typestr);
        logger_->log_error("Failed to convert %s to data type %s", contentstr, typestr);
        session->transfer(flowFile, Failure);
        return;
      }
      logger_->log_trace("Node successfully updated!");
      session->transfer(flowFile, Success);
      return;
    } else {
      logger_->log_trace("Node doesn't exist, trying to create new node");
      std::string browsename;
      if (!context->getProperty(TargetNodeBrowseName, browsename, flowFile)) {
        logger_->log_error("Target node browse name is required for flowfile (%s) as new node is to be created",
                           flowFile->getUUIDStr());
        session->transfer(flowFile, Failure);
        return;
      }
      if (!targetNodeValid) {
        targetnode = UA_NODEID_NUMERIC(1, 0);
      }
      try {
        UA_StatusCode sc;
        UA_NodeId resultnode;
        switch (nodeDataType_) {
          case opc::OPCNodeDataType::Int64: {
            int64_t value = std::stoll(contentstr);
            sc = connection_->add_node(parentNodeID_, targetnode, browsename, value, nodeDataType_, &resultnode);
            break;
          }
          case opc::OPCNodeDataType::UInt64: {
            uint64_t value = std::stoull(contentstr);
            sc = connection_->add_node(parentNodeID_, targetnode, browsename, value, nodeDataType_, &resultnode);
            break;
          }
          case opc::OPCNodeDataType::Int32: {
            int32_t value = std::stoi(contentstr);
            sc = connection_->add_node(parentNodeID_, targetnode, browsename, value, nodeDataType_, &resultnode);
            break;
          }
          case opc::OPCNodeDataType::UInt32: {
            uint32_t value = std::stoul(contentstr);
            sc = connection_->add_node(parentNodeID_, targetnode, browsename, value, nodeDataType_, &resultnode);
            break;
          }
          case opc::OPCNodeDataType::Boolean: {
            const auto contentstr_parsed = utils::StringUtils::toBool(contentstr);
            if (contentstr_parsed) {
              sc = connection_->add_node(parentNodeID_, targetnode, browsename, contentstr_parsed.value(), nodeDataType_, &resultnode);
            } else {
              throw opc::OPCException(GENERAL_EXCEPTION, "Content cannot be converted to bool");
            }
            break;
          }
          case opc::OPCNodeDataType::Float: {
            float value = std::stof(contentstr);
            sc = connection_->add_node(parentNodeID_, targetnode, browsename, value, nodeDataType_, &resultnode);
            break;
          }
          case opc::OPCNodeDataType::Double: {
            double value = std::stod(contentstr);
            sc = connection_->add_node(parentNodeID_, targetnode, browsename, value, nodeDataType_, &resultnode);
            break;
          }
          case opc::OPCNodeDataType::String: {
            sc = connection_->add_node(parentNodeID_, targetnode, browsename, contentstr, nodeDataType_, &resultnode);
            break;
          }
          default:
            throw opc::OPCException(GENERAL_EXCEPTION, "This should never happen!");
        }
        if (sc != UA_STATUSCODE_GOOD) {
          logger_->log_error("Failed to create node: %s", UA_StatusCode_name(sc));
          session->transfer(flowFile, Failure);
          return;
        }
      } catch (...) {
        std::string typestr;
        context->getProperty(ValueType.getName(), typestr);
        logger_->log_error("Failed to convert %s to data type %s", contentstr, typestr);
        session->transfer(flowFile, Failure);
        return;
      }
      logger_->log_trace("Node successfully created!");
      session->transfer(flowFile, Success);
      return;
    }
  }

  int64_t PutOPCProcessor::ReadCallback::process(const std::shared_ptr<io::BaseStream>& stream) {
    buf_.clear();
    buf_.resize(stream->size());

    uint64_t size = 0;

    do {
      const auto read = stream->read(buf_.data() + size, 1024);
      if (io::isError(read)) return -1;
      if (read == 0) break;
      size += read;
    } while (size < stream->size());

    logger_->log_trace("Read %llu bytes from flowfile content to buffer", stream->size());

    return gsl::narrow<int64_t>(size);
  }

REGISTER_RESOURCE(PutOPCProcessor, "Creates/updates  OPC nodes");

} /* namespace processors */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
