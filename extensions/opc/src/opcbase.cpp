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
#include "FlowFileRecord.h"
#include "core/ProcessContext.h"


namespace org::apache::nifi::minifi::processors {

  void BaseOPCProcessor::onSchedule(core::ProcessContext& context, core::ProcessSessionFactory&) {
    logger_->log_trace("BaseOPCProcessor::onSchedule");

    applicationURI_.clear();
    certBuffer_.clear();
    keyBuffer_.clear();
    password_.clear();
    username_.clear();
    trustBuffers_.clear();

    endPointURL_ = context.getProperty(OPCServerEndPoint).value_or("");
    applicationURI_ = context.getProperty(ApplicationURI).value_or("");

    const auto username = context.getProperty(Username);
    const auto password = context.getProperty(Password);

    if (username.has_value() != password.has_value()) {
      throw Exception(PROCESS_SCHEDULE_EXCEPTION, "Both or neither of Username and Password should be provided!");
    }
    username_ = username.value_or("");
    password_ = password.value_or("");

    auto certificatePath = context.getProperty(CertificatePath);
    auto keyPath = context.getProperty(KeyPath);
    trustpath_ = context.getProperty(TrustedPath).value_or("");
    if (certificatePath.has_value() != keyPath.has_value()) {
      throw Exception(PROCESS_SCHEDULE_EXCEPTION, "All or none of Certificate path and Key path should be provided!");
    }
    keypath_ = keyPath.value_or("");
    certpath_ = certificatePath.value_or("");

    if (certpath_.empty()) {
      return;
    }
    if (applicationURI_.empty()) {
      throw Exception(PROCESS_SCHEDULE_EXCEPTION, "Application URI must be provided if Certificate path is provided!");
    }

    std::ifstream input_cert(certpath_, std::ios::binary);
    if (input_cert.good()) {
      certBuffer_ = std::vector<char>(std::istreambuf_iterator<char>(input_cert), {});
    }
    std::ifstream input_key(keypath_, std::ios::binary);
    if (input_key.good()) {
      keyBuffer_ = std::vector<char>(std::istreambuf_iterator<char>(input_key), {});
    }

    if (certBuffer_.empty()) {
      auto error_msg = utils::string::join_pack("Failed to load cert from path: ", certpath_);
      throw Exception(PROCESS_SCHEDULE_EXCEPTION, error_msg);
    }
    if (keyBuffer_.empty()) {
      auto error_msg = utils::string::join_pack("Failed to load key from path: ", keypath_);
      throw Exception(PROCESS_SCHEDULE_EXCEPTION, error_msg);
    }

    if (!trustpath_.empty()) {
      std::ifstream input_trust(trustpath_, std::ios::binary);
      if (input_trust.good()) {
        trustBuffers_.push_back(std::vector<char>(std::istreambuf_iterator<char>(input_trust), {}));
      } else {
        auto error_msg = utils::string::join_pack("Failed to load trusted server certs from path: ", trustpath_);
        throw Exception(PROCESS_SCHEDULE_EXCEPTION, error_msg);
      }
    }
  }

  bool BaseOPCProcessor::reconnect() {
    if (connection_ == nullptr) {
      connection_ = opc::Client::createClient(logger_, applicationURI_, certBuffer_, keyBuffer_, trustBuffers_);
    }

    if (connection_->isConnected()) {
      return true;
    }

    auto sc = connection_->connect(endPointURL_, username_, password_);
    if (sc != UA_STATUSCODE_GOOD) {
      logger_->log_error("Failed to connect: {}!", UA_StatusCode_name(sc));
      return false;
    }
    logger_->log_debug("Successfully connected.");
    return true;
  }

}  // namespace org::apache::nifi::minifi::processors
