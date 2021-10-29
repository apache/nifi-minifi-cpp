/**
 * @file KafkaProcessorBase.h
 * KafkaProcessorBase class declaration
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
#pragma once

#include <optional>

#include "core/Processor.h"
#include "rdkafka_utils.h"

namespace org::apache::nifi::minifi::processors {

// PublishKafka Class
class KafkaProcessorBase : public core::Processor {
 public:
  EXTENSIONAPI static const core::Property SSLContextService;
  EXTENSIONAPI static const core::Property SecurityProtocol;
  EXTENSIONAPI static const core::Property KerberosServiceName;
  EXTENSIONAPI static const core::Property KerberosPrincipal;
  EXTENSIONAPI static const core::Property KerberosKeytabPath;
  EXTENSIONAPI static const core::Property SASLMechanism;
  EXTENSIONAPI static const core::Property Username;
  EXTENSIONAPI static const core::Property Password;

  static const std::string SECURITY_PROTOCOL_PLAINTEXT;
  static const std::string SECURITY_PROTOCOL_SSL;
  static const std::string SECURITY_PROTOCOL_SASL_PLAIN;
  static const std::string SECURITY_PROTOCOL_SASL_SSL;
  static const std::string SASL_MECHANISM_GSSAPI;
  static const std::string SASL_MECHANISM_PLAIN;

  KafkaProcessorBase(const std::string& name, const utils::Identifier& uuid, std::shared_ptr<core::logging::Logger> logger)
      : core::Processor(name, uuid),
        logger_(logger) {
  }

 protected:
  virtual std::optional<utils::SSL_data> getSslData(const std::shared_ptr<core::ProcessContext> &context) const;
  void setKafkaAuthenticationParameters(const std::shared_ptr<core::ProcessContext> &context, rd_kafka_conf_t* config);

  std::string security_protocol_;
  std::shared_ptr<core::logging::Logger> logger_;
};

}  // namespace org::apache::nifi::minifi::processors
