// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#include <memory>
#include <string>
#include <utility>

#include "OPCCommon.h"
#include "BaseOPCProcessor.h"
#include "minifi-cpp/FlowFileRecord.h"
#include "core/ProcessSession.h"
#include "core/Core.h"
#include "utils/ProcessorConfigUtils.h"

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
    connection_ = opc::Client::createClient(logger_, application_uri_, cert_buffer_, key_buffer_, trust_buffers_);
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

  // A new or re-established connection may be to a restarted server that can reassign node ids
  resolved_path_node_ids_.reset();
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
      resolved_path_node_ids_.reset();
      break;
    default:
      throw Exception(PROCESS_SCHEDULE_EXCEPTION, fmt::format("Unsupported Node ID type: {}", magic_enum::enum_name(id_type_)));
  }
}

const std::vector<opc::NodeId>* BaseOPCProcessor::resolvePathNodeIds() {
  if (!resolved_path_node_ids_) {
    std::vector<opc::NodeId> translated_node_ids;
    auto sc = connection_->translateBrowsePathsToNodeIdsRequest(node_id_, translated_node_ids, namespace_idx_, path_reference_types_, logger_);
    if (sc != UA_STATUSCODE_GOOD) {
      logger_->log_error("Failed to translate path '{}' to a node id: {}", node_id_, UA_StatusCode_name(sc));
      return nullptr;
    }
    resolved_path_node_ids_ = std::move(translated_node_ids);
  }
  return &*resolved_path_node_ids_;
}

const opc::NodeId* BaseOPCProcessor::resolveNodeId() {
  if (id_type_ != opc::OPCNodeIDType::Path) {
    return &node_;
  }
  const auto* node_ids = resolvePathNodeIds();
  if (!node_ids) {
    return nullptr;
  }
  if (node_ids->size() != 1) {
    logger_->log_error("Path '{}' resolved to {} node ids; exactly one node is required", node_id_, node_ids->size());
    resolved_path_node_ids_.reset();  // retry the translation on the next call, the server's address space may change
    return nullptr;
  }
  return &node_ids->front();
}

}  // namespace org::apache::nifi::minifi::processors
