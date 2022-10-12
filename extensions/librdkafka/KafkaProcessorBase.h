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
#pragma once

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "core/Processor.h"
#include "rdkafka_utils.h"
#include "utils/Enum.h"
#include "utils/net/Ssl.h"

namespace org::apache::nifi::minifi::processors {

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
  static auto properties() {
    return std::array{
      SSLContextService,
      SecurityProtocol,
      KerberosServiceName,
      KerberosPrincipal,
      KerberosKeytabPath,
      SASLMechanism,
      Username,
      Password
    };
  }

  SMART_ENUM(SecurityProtocolOption,
    (PLAINTEXT, "plaintext"),
    (SSL, "ssl"),
    (SASL_PLAIN, "sasl_plaintext"),
    (SASL_SSL, "sasl_ssl")
  )

  SMART_ENUM(SASLMechanismOption,
    (GSSAPI, "GSSAPI"),
    (PLAIN, "PLAIN")
  )

  KafkaProcessorBase(std::string name, const utils::Identifier& uuid, std::shared_ptr<core::logging::Logger> logger)
      : core::Processor(std::move(name), uuid),
        logger_(logger) {
  }

 protected:
  virtual std::optional<utils::net::SslData> getSslData(core::ProcessContext& context) const;
  void setKafkaAuthenticationParameters(core::ProcessContext& context, gsl::not_null<rd_kafka_conf_t*> config);

  SecurityProtocolOption security_protocol_;
  std::shared_ptr<core::logging::Logger> logger_;
};

}  // namespace org::apache::nifi::minifi::processors
