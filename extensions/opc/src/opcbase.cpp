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
#include "opcbase.h"
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
  return true;
}

void BaseOPCProcessor::readPathReferenceTypes(core::ProcessContext& context, const std::string& node_id) {
  const auto value = context.getProperty(PathReferenceTypes).value_or("");
  if (value.empty()) {
    return;
  }
  auto path_reference_types = utils::string::splitAndTrimRemovingEmpty(value, "/");
  if (path_reference_types.size() + 1 != utils::string::splitAndTrimRemovingEmpty(node_id, "/").size()) {
    throw Exception(PROCESS_SCHEDULE_EXCEPTION, "Path reference types must be provided for each node pair in the path!");
  }
  for (const auto& reference_type : path_reference_types) {
    if (auto ua_ref_type = opc::mapOpcReferenceType(reference_type)) {
      path_reference_types_.push_back(*ua_ref_type);
    } else {
      throw Exception(PROCESS_SCHEDULE_EXCEPTION, fmt::format("Unsupported reference type set in 'Path reference types' property: '{}'.", reference_type));
    }
  }
}

void BaseOPCProcessor::parseIdType(core::ProcessContext& context, const core::PropertyReference& prop) {
  id_type_ = utils::parseEnumProperty<opc::OPCNodeIDType>(context, prop);

  if (id_type_ == opc::OPCNodeIDType::Int) {
    try {
      static_cast<void>(std::stoi(node_id_));
    } catch(const std::exception&) {
      auto error_msg = utils::string::join_pack(node_id_, " cannot be used as an int type node ID");
      throw Exception(PROCESS_SCHEDULE_EXCEPTION, error_msg);
    }
  }
}

}  // namespace org::apache::nifi::minifi::processors
