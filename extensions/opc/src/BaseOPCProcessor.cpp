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
#include <utility>
#include <vector>

#include "OPCCommon.h"
#include "BaseOPCProcessor.h"
#include "minifi-cpp/FlowFileRecord.h"
#include "core/ProcessSession.h"
#include "core/Core.h"
#include "utils/ProcessorConfigUtils.h"
#include "utils/StringUtils.h"

namespace org::apache::nifi::minifi::processors {

void BaseOPCProcessor::onSchedule(core::ProcessContext& context, core::ProcessSessionFactory&) {
  logger_->log_trace("BaseOPCProcessor::onSchedule");

  application_uri_.clear();
  cert_buffer_.clear();
  key_buffer_.clear();
  password_.clear();
  username_.clear();
  trust_buffers_.clear();

  endpoint_url_ = context.getProperty(OPCServerEndPoint).value_or("");
  application_uri_ = context.getProperty(ApplicationURI).value_or("");
  username_ = context.getProperty(Username).value_or("");
  password_ = context.getProperty(Password).value_or("");

  if (username_.empty() != password_.empty()) {
    throw Exception(PROCESS_SCHEDULE_EXCEPTION, "Both or neither of Username and Password should be provided!");
  }

  certpath_ = context.getProperty(CertificatePath).value_or("");
  keypath_ = context.getProperty(KeyPath).value_or("");
  trustpath_ = context.getProperty(TrustedPath).value_or("");
  if (certpath_.empty() != keypath_.empty()) {
    throw Exception(PROCESS_SCHEDULE_EXCEPTION, "All or none of Certificate path and Key path should be provided!");
  }

  if (certpath_.empty()) {
    return;
  }
  if (application_uri_.empty()) {
    throw Exception(PROCESS_SCHEDULE_EXCEPTION, "Application URI must be provided if Certificate path is provided!");
  }

  std::ifstream input_cert(certpath_, std::ios::binary);
  if (input_cert.good()) {
    cert_buffer_ = std::vector<char>(std::istreambuf_iterator<char>(input_cert), {});
  }
  std::ifstream input_key(keypath_, std::ios::binary);
  if (input_key.good()) {
    key_buffer_ = std::vector<char>(std::istreambuf_iterator<char>(input_key), {});
  }

  if (cert_buffer_.empty()) {
    auto error_msg = utils::string::join_pack("Failed to load cert from path: ", certpath_);
    throw Exception(PROCESS_SCHEDULE_EXCEPTION, error_msg);
  }
  if (key_buffer_.empty()) {
    auto error_msg = utils::string::join_pack("Failed to load key from path: ", keypath_);
    throw Exception(PROCESS_SCHEDULE_EXCEPTION, error_msg);
  }

  auto trusted_cert_paths = utils::string::splitAndTrimRemovingEmpty(trustpath_, ",");
  for (const auto& trust_path : trusted_cert_paths) {
    std::ifstream input_trust(trust_path, std::ios::binary);
    if (input_trust.good()) {
      trust_buffers_.push_back(std::vector<char>(std::istreambuf_iterator<char>(input_trust), {}));
    } else {
      auto error_msg = utils::string::join_pack("Failed to load trusted server certs from path: ", trust_path);
      throw Exception(PROCESS_SCHEDULE_EXCEPTION, error_msg);
    }
  }
}

bool BaseOPCProcessor::reconnect() {
  if (connection_ == nullptr) {
    connection_ = opc::Client::createClient(logger_, application_uri_, cert_buffer_, key_buffer_, trust_buffers_, max_event_queue_size_);
  }

  if (connection_->isConnected()) {
    return true;
  }

  auto sc = connection_->connect(endpoint_url_, username_, password_);
  if (sc != UA_STATUSCODE_GOOD) {
    logger_->log_error("Failed to connect: {}!", UA_StatusCode_name(sc));
    return false;
  }
  logger_->log_debug("Successfully connected.");

  // A new or re-established connection may be to a restarted server that can reassign node ids,
  // so if path is used to identify node id, the cached node id should be invalidated
  path_node_id_resolved_ = false;
  return true;
}

void BaseOPCProcessor::readPathReferenceTypes(core::ProcessContext& context, const core::PropertyReference& path_reference_types_property, const std::string& node_id) {
  auto path_reference_types = opc::parsePathReferenceTypes(context.getProperty(path_reference_types_property).value_or(""), node_id);
  if (!path_reference_types) {
    throw Exception(PROCESS_SCHEDULE_EXCEPTION, path_reference_types.error());
  }
  path_reference_types_ = std::move(*path_reference_types);
}

void BaseOPCProcessor::parseNode(core::ProcessContext& context, const core::PropertyReference& path_reference_types_property) {
  switch (id_type_) {
    case opc::OPCNodeIDType::String:
    case opc::OPCNodeIDType::Int:
    case opc::OPCNodeIDType::Guid: {
      if (auto result = opc::buildNodeId(id_type_, namespace_idx_, node_id_)) {
        node_ = std::move(*result);
      } else {
        throw Exception(PROCESS_SCHEDULE_EXCEPTION, result.error());
      }
      break;
    }
    case opc::OPCNodeIDType::Path:
      path_reference_types_.clear();
      readPathReferenceTypes(context, path_reference_types_property, node_id_);
      path_node_id_resolved_ = false;
      break;
    default:
      throw Exception(PROCESS_SCHEDULE_EXCEPTION, fmt::format("Unsupported Node ID type: {}", magic_enum::enum_name(id_type_)));
  }
}

std::expected<void, std::string> BaseOPCProcessor::resolvePathNode() {
  if (id_type_ != opc::OPCNodeIDType::Path || path_node_id_resolved_) {
    return {};
  }
  auto node = resolveSingleNodePath(node_id_, namespace_idx_, path_reference_types_);
  if (!node) {
    return std::unexpected{node.error()};
  }
  node_ = std::move(*node);
  path_node_id_resolved_ = true;
  return {};
}

std::expected<BaseOPCProcessor::TargetNode, std::string> BaseOPCProcessor::resolveTargetNode(core::ProcessContext& context,
    const core::FlowFile& flow_file, const TargetNodeProperties& properties) const {
  const auto namespace_idx_str = context.getProperty(properties.namespace_index, &flow_file).value_or("");
  UA_UInt16 namespace_idx = 0;
  try {
    namespace_idx = gsl::narrow<UA_UInt16>(std::stoul(namespace_idx_str));
  } catch (const std::exception&) {
    return std::unexpected{fmt::format("invalid target node namespace index '{}'", namespace_idx_str)};
  }

  const auto id_type_str = context.getProperty(properties.id_type, &flow_file).value_or("");
  const auto id_type = magic_enum::enum_cast<opc::OPCNodeIDType>(id_type_str);
  if (!id_type) {
    return std::unexpected{fmt::format("invalid target node ID type '{}'", id_type_str)};
  }

  const auto id = context.getProperty(properties.id, &flow_file).value_or("");
  if (id.empty()) {
    return std::unexpected{"target node ID is empty"};
  }

  // A path is resolved by browsing existing nodes, so it cannot point to a node that is yet to be created
  if (*id_type == opc::OPCNodeIDType::Path) {
    if (utils::string::splitAndTrimRemovingEmpty(id, "/").empty()) {
      return std::unexpected{fmt::format("invalid target node path '{}'", id)};
    }
    auto reference_types = opc::parsePathReferenceTypes(context.getProperty(properties.path_reference_types, &flow_file).value_or(""), id);
    if (!reference_types) {
      return std::unexpected{fmt::format("invalid target node path reference types: {}", reference_types.error())};
    }
    auto node = resolveSingleNodePath(id, namespace_idx, *reference_types);
    if (!node) {
      return std::unexpected{fmt::format("target node {}; Path target node ID type can only be used to update existing nodes", node.error())};
    }
    return TargetNode{.node_id = std::move(*node), .exists = true};
  }

  auto node = opc::buildNodeId(*id_type, namespace_idx, id);
  if (!node) {
    return std::unexpected{node.error()};
  }
  const bool exists = connection_->exists(*node);
  return TargetNode{.node_id = std::move(*node), .exists = exists};
}

std::expected<opc::NodeId, std::string> BaseOPCProcessor::resolveSingleNodePath(const std::string& path, UA_UInt16 namespace_idx,
    const std::vector<UA_UInt32>& reference_types) const {
  std::vector<opc::NodeId> translated_node_ids;
  if (auto sc = connection_->translateBrowsePathsToNodeIdsRequest(path, translated_node_ids, namespace_idx, reference_types, logger_); sc != UA_STATUSCODE_GOOD) {
    return std::unexpected{fmt::format("path '{}' could not be translated to a node id: {}", path, UA_StatusCode_name(sc))};
  }
  if (translated_node_ids.size() != 1) {
    return std::unexpected{fmt::format("path '{}' resolved to {} node ids, exactly one is required", path, translated_node_ids.size())};
  }
  return std::move(translated_node_ids[0]);
}

}  // namespace org::apache::nifi::minifi::processors
