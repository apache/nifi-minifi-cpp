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

#include <memory>
#include <string>

#include "opc.h"
#include "putopc.h"
#include "core/ProcessSession.h"
#include "core/Resource.h"
#include "utils/StringUtils.h"
#include "core/ProcessContext.h"

namespace org::apache::nifi::minifi::processors {

  void PutOPCProcessor::initialize() {
    setSupportedProperties(Properties);
    setSupportedRelationships(Relationships);
  }

  void PutOPCProcessor::onSchedule(core::ProcessContext& context, core::ProcessSessionFactory& session_factory) {
    logger_->log_trace("PutOPCProcessor::onSchedule");

    parentExists_ = false;

    BaseOPCProcessor::onSchedule(context, session_factory);

    std::string value;
    context.getProperty(ParentNodeID, nodeID_);
    context.getProperty(ParentNodeIDType, value);

    if (value == "String") {
      idType_ = opc::OPCNodeIDType::String;
    } else if (value == "Int") {
      idType_ = opc::OPCNodeIDType::Int;
    } else if (value == "Path") {
      idType_ = opc::OPCNodeIDType::Path;
    } else {
      // Where have our validators gone?
      auto error_msg = utils::string::join_pack(value, " is not a valid node ID type!");
      throw Exception(PROCESS_SCHEDULE_EXCEPTION, error_msg);
    }

    if (idType_ == opc::OPCNodeIDType::Int) {
      try {
        // ensure that nodeID_ can be parsed as an int
        static_cast<void>(std::stoi(nodeID_));
      } catch(...) {
        auto error_msg = utils::string::join_pack(nodeID_, " cannot be used as an int type node ID");
        throw Exception(PROCESS_SCHEDULE_EXCEPTION, error_msg);
      }
    }
    if (idType_ != opc::OPCNodeIDType::Path) {
      if (!context.getProperty(ParentNameSpaceIndex, nameSpaceIdx_)) {
        auto error_msg = utils::string::join_pack(ParentNameSpaceIndex.name, " is mandatory in case ", ParentNodeIDType.name, " is not Path");
        throw Exception(PROCESS_SCHEDULE_EXCEPTION, error_msg);
      }
    }

    std::string typestr;
    context.getProperty(ValueType, typestr);
    nodeDataType_ = utils::at(opc::StringToOPCDataTypeMap, typestr);  // This throws, but allowed values are generated based on this map -> that's a really unexpected error
  }

  void PutOPCProcessor::onTrigger(core::ProcessContext& context, core::ProcessSession& session) {
    logger_->log_trace("PutOPCProcessor::onTrigger");

    if (!reconnect()) {
      yield();
      return;
    }

    if (!parentExists_) {
      if (idType_ == opc::OPCNodeIDType::Path) {
        std::vector<UA_NodeId> translatedNodeIDs;
        if (connection_->translateBrowsePathsToNodeIdsRequest(nodeID_, translatedNodeIDs, logger_) !=
            UA_STATUSCODE_GOOD) {
          logger_->log_error("Failed to translate {} to node id, no flow files will be put", nodeID_.c_str());
          yield();
          return;
        } else if (translatedNodeIDs.size() != 1) {
          logger_->log_error("{} was translated to multiple node ids, no flow files will be put", nodeID_.c_str());
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
          parentNodeID_.identifier.numeric = std::stoi(nodeID_);  // NOLINT(cppcoreguidelines-pro-type-union-access)
        } else {  // idType_ == opc::OPCNodeIDType::String
          parentNodeID_.identifierType = UA_NODEIDTYPE_STRING;
          parentNodeID_.identifier.string = UA_STRING_ALLOC(nodeID_.c_str());  // NOLINT(cppcoreguidelines-pro-type-union-access)
        }
        if (!connection_->exists(parentNodeID_)) {
          logger_->log_error("Parent node doesn't exist, no flow files will be put");
          yield();
          return;
        }
        parentExists_ = true;
      }
    }

    auto flowFile = session.get();

    // Do nothing if there are no incoming files
    if (!flowFile) {
      return;
    }

    std::string targetidtype;

    bool targetNodeExists = false;
    bool targetNodeValid = false;
    UA_NodeId targetnode;

    if (context.getProperty(TargetNodeIDType, targetidtype, flowFile.get())) {
      std::string targetid;
      std::string namespaceidx;


      if (!context.getProperty(TargetNodeID, targetid, flowFile.get())) {
        logger_->log_error("Flowfile {} had target node ID type specified ({}) without ID, routing to failure!",
                           flowFile->getUUIDStr(), targetidtype);
        session.transfer(flowFile, Failure);
        return;
      }

      if (!context.getProperty(TargetNodeNameSpaceIndex, namespaceidx, flowFile.get())) {
        logger_->log_error("Flowfile {} had target node ID type specified ({}) without namespace index, routing to failure!", flowFile->getUUIDStr(), targetidtype);
        session.transfer(flowFile, Failure);
        return;
      }
      int32_t nsi = 0;
      try {
        nsi = std::stoi(namespaceidx);
      } catch (...) {
        logger_->log_error("Flowfile {} has invalid namespace index ({}), routing to failure!",
                           flowFile->getUUIDStr(), namespaceidx);
        session.transfer(flowFile, Failure);
        return;
      }

      targetnode.namespaceIndex = nsi;
      if (targetidtype == "Int") {
        targetnode.identifierType = UA_NODEIDTYPE_NUMERIC;
        try {
          targetnode.identifier.numeric = std::stoi(targetid);  // NOLINT(cppcoreguidelines-pro-type-union-access)
          targetNodeValid = true;
        } catch (...) {
          logger_->log_error("Flowfile {}: target node ID is not a valid integer: {}. Routing to failure!",
                             flowFile->getUUIDStr(), targetid);
          session.transfer(flowFile, Failure);
          return;
        }
      } else if (targetidtype == "String") {
        targetnode.identifierType = UA_NODEIDTYPE_STRING;
        targetnode.identifier.string = UA_STRING_ALLOC(targetid.c_str());  // NOLINT(cppcoreguidelines-pro-type-union-access)
        targetNodeValid = true;
      } else {
        logger_->log_error("Flowfile {}: target node ID type is invalid: {}. Routing to failure!",
                           flowFile->getUUIDStr(), targetidtype);
        session.transfer(flowFile, Failure);
        return;
      }
      targetNodeExists = connection_->exists(targetnode);
    }

    const auto contentstr = to_string(session.readBuffer(flowFile));
    if (targetNodeExists) {
      logger_->log_trace("Node exists, trying to update it");
      try {
        UA_StatusCode sc = 0;
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
            const auto contentstr_parsed = utils::string::toBool(contentstr);
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
          logger_->log_error("Failed to update node: {}", UA_StatusCode_name(sc));
          session.transfer(flowFile, Failure);
          return;
        }
      } catch (...) {
        std::string typestr;
        context.getProperty(ValueType, typestr);
        logger_->log_error("Failed to convert {} to data type {}", contentstr, typestr);
        session.transfer(flowFile, Failure);
        return;
      }
      logger_->log_trace("Node successfully updated!");
      session.transfer(flowFile, Success);
      return;
    } else {
      logger_->log_trace("Node doesn't exist, trying to create new node");
      std::string browsename;
      if (!context.getProperty(TargetNodeBrowseName, browsename, flowFile.get())) {
        logger_->log_error("Target node browse name is required for flowfile ({}) as new node is to be created",
                           flowFile->getUUIDStr());
        session.transfer(flowFile, Failure);
        return;
      }
      if (!targetNodeValid) {
        targetnode = UA_NODEID_NUMERIC(1, 0);
      }
      try {
        UA_StatusCode sc = 0;
        UA_NodeId resultnode;
        switch (nodeDataType_) {
          case opc::OPCNodeDataType::Int64: {
            int64_t value = std::stoll(contentstr);
            sc = connection_->add_node(parentNodeID_, targetnode, browsename, value, &resultnode);
            break;
          }
          case opc::OPCNodeDataType::UInt64: {
            uint64_t value = std::stoull(contentstr);
            sc = connection_->add_node(parentNodeID_, targetnode, browsename, value, &resultnode);
            break;
          }
          case opc::OPCNodeDataType::Int32: {
            int32_t value = std::stoi(contentstr);
            sc = connection_->add_node(parentNodeID_, targetnode, browsename, value, &resultnode);
            break;
          }
          case opc::OPCNodeDataType::UInt32: {
            uint32_t value = std::stoul(contentstr);
            sc = connection_->add_node(parentNodeID_, targetnode, browsename, value, &resultnode);
            break;
          }
          case opc::OPCNodeDataType::Boolean: {
            const auto contentstr_parsed = utils::string::toBool(contentstr);
            if (contentstr_parsed) {
              sc = connection_->add_node(parentNodeID_, targetnode, browsename, contentstr_parsed.value(), &resultnode);
            } else {
              throw opc::OPCException(GENERAL_EXCEPTION, "Content cannot be converted to bool");
            }
            break;
          }
          case opc::OPCNodeDataType::Float: {
            float value = std::stof(contentstr);
            sc = connection_->add_node(parentNodeID_, targetnode, browsename, value, &resultnode);
            break;
          }
          case opc::OPCNodeDataType::Double: {
            double value = std::stod(contentstr);
            sc = connection_->add_node(parentNodeID_, targetnode, browsename, value, &resultnode);
            break;
          }
          case opc::OPCNodeDataType::String: {
            sc = connection_->add_node(parentNodeID_, targetnode, browsename, contentstr, &resultnode);
            break;
          }
          default:
            throw opc::OPCException(GENERAL_EXCEPTION, "This should never happen!");
        }
        if (sc != UA_STATUSCODE_GOOD) {
          logger_->log_error("Failed to create node: {}", UA_StatusCode_name(sc));
          session.transfer(flowFile, Failure);
          return;
        }
      } catch (...) {
        std::string typestr;
        context.getProperty(ValueType, typestr);
        logger_->log_error("Failed to convert {} to data type {}", contentstr, typestr);
        session.transfer(flowFile, Failure);
        return;
      }
      logger_->log_trace("Node successfully created!");
      session.transfer(flowFile, Success);
      return;
    }
  }

REGISTER_RESOURCE(PutOPCProcessor, Processor);

}  // namespace org::apache::nifi::minifi::processors
