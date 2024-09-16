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
#include "utils/net/Ssl.h"
#include "controllers/SSLContextService.h"

namespace org::apache::nifi::minifi::utils::net {

std::optional<utils::net::SslData> getSslData(const core::ProcessContext& context, const core::PropertyReference& ssl_prop, const std::shared_ptr<core::logging::Logger>& logger) {
  auto getSslContextService = [&]() -> std::shared_ptr<minifi::controllers::SSLContextService> {
    if (auto ssl_service_name = context.getProperty(ssl_prop); ssl_service_name && !ssl_service_name->empty()) {
      if (auto service = context.getControllerService(*ssl_service_name)) {
        if (auto ssl_service = std::dynamic_pointer_cast<org::apache::nifi::minifi::controllers::SSLContextService>(service)) {
          return ssl_service;
        } else {
          logger->log_warn("SSL Context Service property is set to '{}', but it is not a valid SSLContextService.", *ssl_service_name);
        }
      } else {
        logger->log_warn("SSL Context Service property is set to '{}', but the controller service could not be found.", *ssl_service_name);
      }
    } else {
      logger->log_warn("No valid SSL Context Service property is set.");
    }
    return nullptr;
  };

  if (auto ssl_service = getSslContextService()) {
    utils::net::SslData ssl_data;
    ssl_data.ca_loc = ssl_service->getCACertificate();
    ssl_data.cert_loc = ssl_service->getCertificateFile();
    ssl_data.key_loc = ssl_service->getPrivateKeyFile();
    ssl_data.key_pw = ssl_service->getPassphrase();
    return ssl_data;
  }

  return std::nullopt;
}

}  // namespace org::apache::nifi::minifi::utils::net
