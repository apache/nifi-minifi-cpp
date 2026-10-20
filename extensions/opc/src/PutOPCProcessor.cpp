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
#include <utility>

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
    parseNode(context, PathReferenceTypes);
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

  if (parent_node_defined_) {
    if (auto resolved = resolvePathNode(); !resolved) {
      logger_->log_error("Failed to resolve the parent node: {}", resolved.error());
      context.yield();
      return;
    }
  }

  auto flow_file = session.get();
  if (!flow_file) {
    return;
  }

  const auto target_node = resolveTargetNode(context, *flow_file, {TargetNodeIDType, TargetNodeID, TargetNodeNameSpaceIndex, TargetNodePathReferenceTypes});
  if (!target_node) {
    logger_->log_error("Flowfile {}: {}, routing to failure", flow_file->getUUIDStr(), target_node.error());
    session.transfer(flow_file, Failure);
    return;
  }

  const auto contentstr = to_string(session.readBuffer(flow_file));
  if (target_node->exists) {
    updateNode(target_node->node_id, contentstr, session, flow_file);
  } else if (parent_node_defined_) {
    createNode(target_node->node_id, contentstr, context, session, flow_file);
  } else {
    logger_->log_error("Target node does not exist and no parent node is defined to create it under; routing to failure");
    session.transfer(flow_file, Failure);
  }
}

REGISTER_RESOURCE(PutOPCProcessor, Processor);

}  // namespace org::apache::nifi::minifi::processors
