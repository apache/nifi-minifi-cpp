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
#include "core/Processor.h"
#include "core/ProcessSession.h"
#include "core/Core.h"

namespace org::apache::nifi::minifi::processors {

  void BaseOPCProcessor::onSchedule(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSessionFactory>& /*factory*/) {
    logger_->log_trace("BaseOPCProcessor::onSchedule");

    applicationURI_.clear();
    certBuffer_.clear();
    keyBuffer_.clear();
    password_.clear();
    username_.clear();
    trustBuffers_.clear();

    context->getProperty(OPCServerEndPoint, endPointURL_);
    context->getProperty(ApplicationURI, applicationURI_);

    if (context->getProperty(Username, username_) != context->getProperty(Password, password_)) {
      throw Exception(PROCESS_SCHEDULE_EXCEPTION, "Both or neither of Username and Password should be provided!");
    }

    auto certificatePathRes = context->getProperty(CertificatePath, certpath_);
    auto keyPathRes = context->getProperty(KeyPath, keypath_);
    context->getProperty(TrustedPath, trustpath_);
    if (certificatePathRes != keyPathRes) {
      throw Exception(PROCESS_SCHEDULE_EXCEPTION, "All or none of Certificate path and Key path should be provided!");
    }

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
      auto error_msg = utils::StringUtils::join_pack("Failed to load cert from path: ", certpath_);
      throw Exception(PROCESS_SCHEDULE_EXCEPTION, error_msg);
    }
    if (keyBuffer_.empty()) {
      auto error_msg = utils::StringUtils::join_pack("Failed to load key from path: ", keypath_);
      throw Exception(PROCESS_SCHEDULE_EXCEPTION, error_msg);
    }

    if (!trustpath_.empty()) {
      std::ifstream input_trust(trustpath_, std::ios::binary);
      if (input_trust.good()) {
        trustBuffers_.push_back(std::vector<char>(std::istreambuf_iterator<char>(input_trust), {}));
      } else {
        auto error_msg = utils::StringUtils::join_pack("Failed to load trusted server certs from path: ", trustpath_);
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
      logger_->log_error("Failed to connect: %s!", UA_StatusCode_name(sc));
      return false;
    }
    logger_->log_debug("Successfully connected.");
    return true;
  }

}  // namespace org::apache::nifi::minifi::processors
