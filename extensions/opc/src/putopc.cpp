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

#include "putopc.h"

#include <memory>
#include <string>

#include "minifi-cpp/core/ProcessContext.h"
#include "core/ProcessSession.h"
#include "core/Resource.h"
#include "opc.h"
#include "utils/StringUtils.h"
#include "utils/ProcessorConfigUtils.h"

namespace org::apache::nifi::minifi::processors {

void PutOPCProcessor::initialize() {
  setSupportedProperties(Properties);
  setSupportedRelationships(Relationships);
}

void PutOPCProcessor::onSchedule(core::ProcessContext& context, core::ProcessSessionFactory& session_factory) {
  logger_->log_trace("PutOPCProcessor::onSchedule");

  BaseOPCProcessor::onSchedule(context, session_factory);

  node_id_ = utils::parseProperty(context, ParentNodeID);

  parseIdType(context, ParentNodeIDType);

  namespace_idx_ = gsl::narrow<int32_t>(utils::parseI64Property(context, ParentNameSpaceIndex));
  node_data_type_ = utils::parseEnumProperty<opc::OPCNodeDataType>(context, ValueType);

  if (id_type_ == opc::OPCNodeIDType::Path) {
    readPathReferenceTypes(context, node_id_);
  }

  const auto value = context.getProperty(CreateNodeReferenceType).value_or("");
  if (auto ref_type = opc::mapOpcReferenceType(value)) {
    create_node_reference_type_ = ref_type.value();
  } else {
    logger_->log_error("Invalid reference type: {}", value);
  }
}

bool PutOPCProcessor::readParentNodeId() {
  if (id_type_ == opc::OPCNodeIDType::Path) {
    std::vector<UA_NodeId> translated_node_ids;
    if (connection_->translateBrowsePathsToNodeIdsRequest(node_id_, translated_node_ids, namespace_idx_, path_reference_types_, logger_) !=
        UA_STATUSCODE_GOOD) {
      logger_->log_error("Failed to translate {} to node id, no flow files will be put", node_id_.c_str());
      return false;
    } else if (translated_node_ids.size() != 1) {
      logger_->log_error("{} was translated to multiple node ids, no flow files will be put", node_id_.c_str());
      return false;
    } else {
      parent_node_id_ = translated_node_ids[0];
    }
  } else {
    parent_node_id_.namespaceIndex = namespace_idx_;
    if (id_type_ == opc::OPCNodeIDType::Int) {
      parent_node_id_.identifierType = UA_NODEIDTYPE_NUMERIC;
      parent_node_id_.identifier.numeric = std::stoi(node_id_);  // NOLINT(cppcoreguidelines-pro-type-union-access)
    } else {  // idType_ == opc::OPCNodeIDType::String
      parent_node_id_.identifierType = UA_NODEIDTYPE_STRING;
      parent_node_id_.identifier.string = UA_STRING_ALLOC(node_id_.c_str());  // NOLINT(cppcoreguidelines-pro-type-union-access)
    }
    if (!connection_->exists(parent_node_id_)) {
      logger_->log_error("Parent node doesn't exist, no flow files will be put");
      return false;
    }
  }
  return true;
}

nonstd::expected<std::pair<bool, UA_NodeId>, std::string> PutOPCProcessor::configureTargetNode(core::ProcessContext& context, core::FlowFile& flow_file) const {
  const auto namespaceidx = context.getProperty(TargetNodeNameSpaceIndex, &flow_file).value_or("");
  if (namespaceidx.empty()) {
    return nonstd::make_unexpected(fmt::format("Flowfile {} had no target namespace index specified, routing to failure!", flow_file.getUUIDStr()));
  }
  int32_t nsi = 0;
  try {
    nsi = std::stoi(namespaceidx);
  } catch (const std::exception&) {
    return nonstd::make_unexpected(fmt::format("Flowfile {} has invalid namespace index ({}), routing to failure!",
                                   flow_file.getUUIDStr(), namespaceidx));
  }

  const auto target_id_type = context.getProperty(TargetNodeIDType, &flow_file).value_or("");
  if (target_id_type.empty()) {
    return nonstd::make_unexpected(fmt::format("Flowfile {} has invalid target node id type, routing to failure!",
                                   flow_file.getUUIDStr()));
  }

  const auto target_id = context.getProperty(TargetNodeID, &flow_file).value_or("");
  if (target_id.empty()) {
    return nonstd::make_unexpected(fmt::format("Flowfile {} had target node ID type specified ({}) without ID, routing to failure!",
                                    flow_file.getUUIDStr(), target_id_type));
  }

  UA_NodeId target_node;
  target_node.namespaceIndex = nsi;
  if (target_id_type == "Int") {
    target_node.identifierType = UA_NODEIDTYPE_NUMERIC;
    try {
      target_node.identifier.numeric = std::stoi(target_id);  // NOLINT(cppcoreguidelines-pro-type-union-access)
    } catch (const std::exception&) {
      return nonstd::make_unexpected(fmt::format("Flowfile {}: target node ID is not a valid integer: {}. Routing to failure!",
                                      flow_file.getUUIDStr(), target_id));
    }
  } else if (target_id_type == "String") {
    target_node.identifierType = UA_NODEIDTYPE_STRING;
    target_node.identifier.string = UA_STRING_ALLOC(target_id.c_str());  // NOLINT(cppcoreguidelines-pro-type-union-access)
  } else {
    return nonstd::make_unexpected(fmt::format("Flowfile {}: target node ID type is invalid: {}. Routing to failure!",
                                    flow_file.getUUIDStr(), target_id_type));
  }
  return std::make_pair(connection_->exists(target_node), target_node);
}

void PutOPCProcessor::updateNode(const UA_NodeId& target_node, const std::string& contentstr, core::ProcessSession& session, const std::shared_ptr<core::FlowFile>& flow_file) const {
  logger_->log_trace("Node exists, trying to update it");
  try {
    UA_StatusCode sc = 0;
    switch (node_data_type_) {
      case opc::OPCNodeDataType::Int64: {
        int64_t value = std::stoll(contentstr);
        sc = connection_->update_node(target_node, value);
        break;
      }
      case opc::OPCNodeDataType::UInt64: {
        uint64_t value = std::stoull(contentstr);
        sc = connection_->update_node(target_node, value);
        break;
      }
      case opc::OPCNodeDataType::Int32: {
        int32_t value = std::stoi(contentstr);
        sc = connection_->update_node(target_node, value);
        break;
      }
      case opc::OPCNodeDataType::UInt32: {
        uint32_t value = std::stoul(contentstr);
        sc = connection_->update_node(target_node, value);
        break;
      }
      case opc::OPCNodeDataType::Boolean: {
        if (auto contentstr_parsed = utils::string::toBool(contentstr)) {
          sc = connection_->update_node(target_node, contentstr_parsed.value());
        } else {
          throw std::runtime_error("Content cannot be converted to bool");
        }
        break;
      }
      case opc::OPCNodeDataType::Float: {
        float value = std::stof(contentstr);
        sc = connection_->update_node(target_node, value);
        break;
      }
      case opc::OPCNodeDataType::Double: {
        double value = std::stod(contentstr);
        sc = connection_->update_node(target_node, value);
        break;
      }
      case opc::OPCNodeDataType::String: {
        sc = connection_->update_node(target_node, contentstr);
        break;
      }
      default:
        logger_->log_error("Unhandled data type: {}", magic_enum::enum_name(node_data_type_));
        gsl_Assert(false);
    }
    if (sc != UA_STATUSCODE_GOOD) {
      logger_->log_error("Failed to update node: {}", UA_StatusCode_name(sc));
      session.transfer(flow_file, Failure);
      return;
    }

    logger_->log_trace("Node successfully updated!");
    session.transfer(flow_file, Success);
  } catch (const std::exception&) {
    logger_->log_error("Failed to convert {} to data type {}", contentstr, magic_enum::enum_name(node_data_type_));
    session.transfer(flow_file, Failure);
  }
}

void PutOPCProcessor::createNode(const UA_NodeId& target_node, const std::string& contentstr, core::ProcessContext& context, core::ProcessSession& session,
    const std::shared_ptr<core::FlowFile>& flow_file) const {
  logger_->log_trace("Node doesn't exist, trying to create new node");
  const auto browse_name = context.getProperty(TargetNodeBrowseName, flow_file.get()).value_or("");
  if (browse_name.empty()) {
    logger_->log_error("Target node browse name is required for flowfile ({}) as new node is to be created",
                       flow_file->getUUIDStr());
    session.transfer(flow_file, Failure);
    return;
  }

  try {
    UA_StatusCode sc = 0;
    UA_NodeId result_node;
    switch (node_data_type_) {
      case opc::OPCNodeDataType::Int64: {
        int64_t value = std::stoll(contentstr);
        sc = connection_->add_node(parent_node_id_, target_node, create_node_reference_type_, browse_name, value, &result_node);
        break;
      }
      case opc::OPCNodeDataType::UInt64: {
        uint64_t value = std::stoull(contentstr);
        sc = connection_->add_node(parent_node_id_, target_node, create_node_reference_type_, browse_name, value, &result_node);
        break;
      }
      case opc::OPCNodeDataType::Int32: {
        int32_t value = std::stoi(contentstr);
        sc = connection_->add_node(parent_node_id_, target_node, create_node_reference_type_, browse_name, value, &result_node);
        break;
      }
      case opc::OPCNodeDataType::UInt32: {
        uint32_t value = std::stoul(contentstr);
        sc = connection_->add_node(parent_node_id_, target_node, create_node_reference_type_, browse_name, value, &result_node);
        break;
      }
      case opc::OPCNodeDataType::Boolean: {
        if (auto contentstr_parsed = utils::string::toBool(contentstr)) {
          sc = connection_->add_node(parent_node_id_, target_node, create_node_reference_type_, browse_name, contentstr_parsed.value(), &result_node);
        } else {
          throw std::runtime_error("Content cannot be converted to bool");
        }
        break;
      }
      case opc::OPCNodeDataType::Float: {
        float value = std::stof(contentstr);
        sc = connection_->add_node(parent_node_id_, target_node, create_node_reference_type_, browse_name, value, &result_node);
        break;
      }
      case opc::OPCNodeDataType::Double: {
        double value = std::stod(contentstr);
        sc = connection_->add_node(parent_node_id_, target_node, create_node_reference_type_, browse_name, value, &result_node);
        break;
      }
      case opc::OPCNodeDataType::String: {
        sc = connection_->add_node(parent_node_id_, target_node, create_node_reference_type_, browse_name, contentstr, &result_node);
        break;
      }
      default:
        logger_->log_error("Unhandled data type: {}", magic_enum::enum_name(node_data_type_));
        gsl_Assert(false);
    }
    if (sc != UA_STATUSCODE_GOOD) {
      logger_->log_error("Failed to create node: {}", UA_StatusCode_name(sc));
      session.transfer(flow_file, Failure);
      return;
    }

    logger_->log_trace("Node successfully created!");
    session.transfer(flow_file, Success);
  } catch (const std::exception&) {
    logger_->log_error("Failed to convert {} to data type {}", contentstr, magic_enum::enum_name(node_data_type_));
    session.transfer(flow_file, Failure);
  }
}

void PutOPCProcessor::onTrigger(core::ProcessContext& context, core::ProcessSession& session) {
  logger_->log_trace("PutOPCProcessor::onTrigger");

  if (!reconnect()) {
    logger_->log_warn("Could not connect to OPC server, yielding");
    context.yield();
    return;
  }

  if (!readParentNodeId()) {
    context.yield();
    return;
  }

  auto flow_file = session.get();
  if (!flow_file) {
    return;
  }

  auto target_node_result = configureTargetNode(context, *flow_file);
  if (!target_node_result.has_value()) {
    logger_->log_error("{}", target_node_result.error());
    session.transfer(flow_file, Failure);
    return;
  }

  const auto& [target_node_exists, target_node] = target_node_result.value();
  const auto contentstr = to_string(session.readBuffer(flow_file));
  if (target_node_exists) {
    updateNode(target_node, contentstr, session, flow_file);
  } else {
    createNode(target_node, contentstr, context, session, flow_file);
  }
}

REGISTER_RESOURCE(PutOPCProcessor, Processor);

}  // namespace org::apache::nifi::minifi::processors
