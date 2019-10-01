/**
 * BaseOPC class definition
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

#include <memory>
#include <string>

#include "opc.h"
#include "opcbase.h"
#include "FlowFileRecord.h"
#include "core/Processor.h"
#include "core/ProcessSession.h"
#include "core/Core.h"
#include "core/Property.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace processors {

  core::Property BaseOPCProcessor::OPCServerEndPoint(
      core::PropertyBuilder::createProperty("OPC server endpoint")
      ->withDescription("Specifies the address, port and relative path of an OPC endpoint")
      ->isRequired(true)->build());

  core::Property BaseOPCProcessor::Username(
      core::PropertyBuilder::createProperty("Username")
          ->withDescription("Username to log in with.")->build());

  core::Property BaseOPCProcessor::Password(
      core::PropertyBuilder::createProperty("Password")
          ->withDescription("Password to log in with. Providing this requires cert and key to be provided as well, credentials are always sent encrypted.")->build());

  core::Property BaseOPCProcessor::CertificatePath(
      core::PropertyBuilder::createProperty("Certificate path")
          ->withDescription("Path to the cert file")->build());

  core::Property BaseOPCProcessor::KeyPath(
      core::PropertyBuilder::createProperty("Key path")
          ->withDescription("Path to the key file")->build());

  core::Property BaseOPCProcessor::TrustedPath(
      core::PropertyBuilder::createProperty("Trusted certificate path")
          ->withDescription("Path to the trusted certificate or trutested certificates directory")->build());

  void BaseOPCProcessor::onSchedule(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSessionFactory> &factory) {
    logger_->log_trace("BaseOPCProcessor::onSchedule");

    certBuffer_.clear();
    keyBuffer_.clear();
    password_.clear();
    username_.clear();
    trustBuffers_.clear();

    configOK_ = false;

    context->getProperty(OPCServerEndPoint.getName(), endPointURL_);

    if (context->getProperty(Username.getName(), username_) != context->getProperty(Password.getName(), password_)) {
      logger_->log_error("Both or neither of username and password should be provided!");
      return;
    }

    auto certificatePathRes = context->getProperty(CertificatePath.getName(), certpath_);
    auto keyPathRes = context->getProperty(KeyPath.getName(), keypath_);
    auto trustedPathRes = context->getProperty(TrustedPath.getName(), trustpath_);
    if (certificatePathRes != keyPathRes || keyPathRes != trustedPathRes) {
      logger_->log_error("All or none of certificate, key and trusted paths should be provided!");
      return;
    }

    if (!password_.empty() && (certpath_.empty() || keypath_.empty() || trustpath_.empty())) {
      logger_->log_error("Certificate, key and trusted paths must be provided in case password is provided!");
      return;
    }

    if (!certpath_.empty()) {
      std::ifstream input_cert(certpath_, std::ios::binary);
      if (input_cert.good()) {
        certBuffer_ = std::vector<char>(std::istreambuf_iterator<char>(input_cert), {});
      }
      std::ifstream input_key(keypath_, std::ios::binary);
      if (input_key.good()) {
        keyBuffer_ = std::vector<char>(std::istreambuf_iterator<char>(input_key), {});
      }

      trustBuffers_.emplace_back();
      std::ifstream input_trust(trustpath_, std::ios::binary);
      if (input_trust.good()) {
        trustBuffers_[0] = std::vector<char>(std::istreambuf_iterator<char>(input_trust), {});
      }

      if (certBuffer_.empty()) {
        logger_->log_error("Failed to load cert from path: %s", certpath_);
        return;
      }
      if (keyBuffer_.empty()) {
        logger_->log_error("Failed to load key from path: %s", keypath_);
        return;
      }

      if (trustBuffers_[0].empty()) {
        logger_->log_error("Failed to load trusted certs from path: %s", trustpath_);
        return;
      }
      certBuffer_.emplace_back('\0');
      keyBuffer_.emplace_back('\0');
      trustBuffers_[0].emplace_back('\0');
    }

    configOK_ = true;
  }

  bool BaseOPCProcessor::reconnect() {
    if (connection_ == nullptr) {
      connection_ = opc::createClient(logger_, certBuffer_, keyBuffer_, trustBuffers_);
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

} /* namespace processors */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
