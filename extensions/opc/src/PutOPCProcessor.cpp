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

#include "PutOPCProcessor.h"

#include <memory>
#include <string>

#include "OPCCommon.h"
#include "core/ProcessSession.h"
#include "core/Resource.h"
#include "minifi-cpp/core/ProcessContext.h"
#include "utils/ProcessorConfigUtils.h"
#include "utils/StringUtils.h"

namespace org::apache::nifi::minifi::processors {

void PutOPCProcessor::initialize() {
  setSupportedProperties(Properties);
  setSupportedRelationships(Relationships);
}

void PutOPCProcessor::onSchedule(core::ProcessContext& context, core::ProcessSessionFactory& session_factory) {
  logger_->log_trace("PutOPCProcessor::onSchedule");

  BaseOPCProcessor::onSchedule(context, session_factory);

  parent_node_defined_ = false;
  if (const auto parent_node_id = utils::parseOptionalProperty(context, ParentNodeID); parent_node_id && !parent_node_id->empty()) {
    node_id_ = *parent_node_id;
    id_type_ = utils::parseEnumProperty<opc::OPCNodeIDType>(context, ParentNodeIDType);
    namespace_idx_ = gsl::narrow<UA_UInt16>(utils::parseOptionalU64Property(context, ParentNameSpaceIndex).value_or(0));
    parseNode(context);
    parent_node_defined_ = true;
  }

  node_data_type_ = utils::parseEnumProperty<opc::OPCNodeDataType>(context, ValueType);

  const auto value = context.getProperty(CreateNodeReferenceType).value_or("");
  if (auto ref_type = opc::mapOpcReferenceType(value)) {
    create_node_reference_type_ = ref_type.value();
  } else {
    logger_->log_error("Invalid reference type: {}", value);
  }
}

std::expected<std::pair<bool, opc::NodeId>, std::string> PutOPCProcessor::configureTargetNode(core::ProcessContext& context,
    core::FlowFile& flow_file) const {
  const auto namespace_idx_str = context.getProperty(TargetNodeNameSpaceIndex, &flow_file).value_or("");
  if (namespace_idx_str.empty()) {
    return std::unexpected{fmt::format("Flowfile {} had no target namespace index specified, routing to failure!", flow_file.getUUIDStr())};
  }
  UA_UInt16 namespace_idx = 0;
  try {
    namespace_idx = gsl::narrow<UA_UInt16>(std::stoi(namespace_idx_str));
  } catch (const std::exception&) {
    return std::unexpected{fmt::format("Flowfile {} has invalid namespace index ({}), routing to failure!", flow_file.getUUIDStr(), namespace_idx_str)};
  }

  const auto target_id_type_str = context.getProperty(TargetNodeIDType, &flow_file).value_or("");
  if (target_id_type_str.empty()) {
    return std::unexpected{fmt::format("Flowfile {} has invalid target node id type, routing to failure!", flow_file.getUUIDStr())};
  }

  auto target_id_type = magic_enum::enum_cast<opc::OPCNodeIDType>(target_id_type_str);
  if (!target_id_type || *target_id_type == opc::OPCNodeIDType::Path) {
    return std::unexpected{fmt::format("Flowfile {} has invalid target node id type '{}', routing to failure!", flow_file.getUUIDStr(), target_id_type_str)};
  }

  const auto target_id = context.getProperty(TargetNodeID, &flow_file).value_or("");
  if (target_id.empty()) {
    return std::unexpected{
        fmt::format("Flowfile {} had target node ID type specified ({}) without ID, routing to failure!", flow_file.getUUIDStr(), target_id_type_str)};
  }

  opc::NodeId target_node;
  if (auto result = opc::buildNodeId(*target_id_type, namespace_idx, target_id)) {
    target_node = std::move(*result);
  } else {
    return std::unexpected{fmt::format("Flowfile {}: {}. Routing to failure!", flow_file.getUUIDStr(), result.error())};
  }
  const bool target_node_exists = connection_->exists(target_node);
  return std::make_pair(target_node_exists, std::move(target_node));
}

void PutOPCProcessor::updateNode(const UA_NodeId& target_node, const std::string& contentstr, core::ProcessSession& session,
    const std::shared_ptr<core::FlowFile>& flow_file) const {
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

void PutOPCProcessor::createNode(const UA_NodeId& target_node, const std::string& contentstr, core::ProcessContext& context,
    core::ProcessSession& session, const std::shared_ptr<core::FlowFile>& flow_file) const {
  logger_->log_trace("Node doesn't exist, trying to create new node");
  const auto browse_name = context.getProperty(TargetNodeBrowseName, flow_file.get()).value_or("");
  if (browse_name.empty()) {
    logger_->log_error("Target node browse name is required for flowfile ({}) as new node is to be created", flow_file->getUUIDStr());
    session.transfer(flow_file, Failure);
    return;
  }

  try {
    UA_StatusCode sc = 0;
    opc::NodeId result_node;
    switch (node_data_type_) {
      case opc::OPCNodeDataType::Int64: {
        int64_t value = std::stoll(contentstr);
        sc = connection_->add_node(node_, target_node, create_node_reference_type_, browse_name, value, result_node.receive());
        break;
      }
      case opc::OPCNodeDataType::UInt64: {
        uint64_t value = std::stoull(contentstr);
        sc = connection_->add_node(node_, target_node, create_node_reference_type_, browse_name, value, result_node.receive());
        break;
      }
      case opc::OPCNodeDataType::Int32: {
        int32_t value = std::stoi(contentstr);
        sc = connection_->add_node(node_, target_node, create_node_reference_type_, browse_name, value, result_node.receive());
        break;
      }
      case opc::OPCNodeDataType::UInt32: {
        uint32_t value = std::stoul(contentstr);
        sc = connection_->add_node(node_, target_node, create_node_reference_type_, browse_name, value, result_node.receive());
        break;
      }
      case opc::OPCNodeDataType::Boolean: {
        if (auto contentstr_parsed = utils::string::toBool(contentstr)) {
          sc = connection_->add_node(node_, target_node, create_node_reference_type_, browse_name, contentstr_parsed.value(), result_node.receive());
        } else {
          throw std::runtime_error("Content cannot be converted to bool");
        }
        break;
      }
      case opc::OPCNodeDataType::Float: {
        float value = std::stof(contentstr);
        sc = connection_->add_node(node_, target_node, create_node_reference_type_, browse_name, value, result_node.receive());
        break;
      }
      case opc::OPCNodeDataType::Double: {
        double value = std::stod(contentstr);
        sc = connection_->add_node(node_, target_node, create_node_reference_type_, browse_name, value, result_node.receive());
        break;
      }
      case opc::OPCNodeDataType::String: {
        sc = connection_->add_node(node_, target_node, create_node_reference_type_, browse_name, contentstr, result_node.receive());
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

  if (parent_node_defined_ && id_type_ == opc::OPCNodeIDType::Path && !path_node_id_resolved_) {
    std::vector<opc::NodeId> translated_node_ids;
    auto sc = connection_->translateBrowsePathsToNodeIdsRequest(node_id_, translated_node_ids, namespace_idx_, path_reference_types_, logger_);
    if (sc != UA_STATUSCODE_GOOD) {
      logger_->log_error("Failed to translate path '{}' to a node id: {}", node_id_, UA_StatusCode_name(sc));
      context.yield();
      return;
    }
    if (translated_node_ids.size() != 1) {
      logger_->log_error("Path '{}' resolved to {} node ids; exactly one target node is required for put", node_id_, translated_node_ids.size());
      context.yield();
      return;
    }
    node_ = std::move(translated_node_ids[0]);
    path_node_id_resolved_ = true;
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
  } else if (parent_node_defined_) {
    createNode(target_node, contentstr, context, session, flow_file);
  } else {
    logger_->log_error("Target node does not exist and no parent node is defined to create it under; routing to failure");
    session.transfer(flow_file, Failure);
  }
}

REGISTER_RESOURCE(PutOPCProcessor, Processor);

}  // namespace org::apache::nifi::minifi::processors
