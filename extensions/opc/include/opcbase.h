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

#include <string>
#include <memory>
#include <utility>
#include <vector>

#include "opc.h"
#include "core/Processor.h"
#include "core/ProcessSession.h"
#include "core/PropertyDefinition.h"
#include "core/PropertyDefinitionBuilder.h"
#include "core/Core.h"
#include "core/Property.h"

namespace org::apache::nifi::minifi::processors {

class BaseOPCProcessor : public core::ProcessorImpl {
 public:
  EXTENSIONAPI static constexpr auto OPCServerEndPoint = core::PropertyDefinitionBuilder<>::createProperty("OPC server endpoint")
      .withDescription("Specifies the address, port and relative path of an OPC endpoint")
      .isRequired(true)
      .build();
  EXTENSIONAPI static constexpr auto ApplicationURI = core::PropertyDefinitionBuilder<>::createProperty("Application URI")
      .withDescription("Application URI of the client in the format 'urn:unconfigured:application'. "
          "Mandatory, if using Secure Channel and must match the URI included in the certificate's Subject Alternative Names.")
      .build();
  EXTENSIONAPI static constexpr auto Username = core::PropertyDefinitionBuilder<>::createProperty("Username")
      .withDescription("Username to log in with.")
      .build();
  EXTENSIONAPI static constexpr auto Password = core::PropertyDefinitionBuilder<>::createProperty("Password")
      .withDescription("Password to log in with.")
      .isSensitive(true)
      .build();
  EXTENSIONAPI static constexpr auto CertificatePath = core::PropertyDefinitionBuilder<>::createProperty("Certificate path")
      .withDescription("Path to the DER-encoded cert file")
      .build();
  EXTENSIONAPI static constexpr auto KeyPath = core::PropertyDefinitionBuilder<>::createProperty("Key path")
      .withDescription("Path to the DER-encoded key file")
      .build();
  EXTENSIONAPI static constexpr auto TrustedPath = core::PropertyDefinitionBuilder<>::createProperty("Trusted server certificate path")
      .withDescription("Path to the DER-encoded trusted server certificate")
      .build();
  EXTENSIONAPI static constexpr auto Properties = std::to_array<core::PropertyReference>({
      OPCServerEndPoint,
      ApplicationURI,
      Username,
      Password,
      CertificatePath,
      KeyPath,
      TrustedPath
  });


  explicit BaseOPCProcessor(std::string_view name, const utils::Identifier& uuid = {})
  : ProcessorImpl(name, uuid) {
  }

  void onSchedule(core::ProcessContext& context, core::ProcessSessionFactory& factory) override;

 protected:
  virtual bool reconnect();

  std::shared_ptr<core::logging::Logger> logger_;

  opc::ClientPtr connection_;

  std::string endPointURL_;

  std::string applicationURI_;
  std::string username_;
  std::string password_;
  std::string certpath_;
  std::string keypath_;
  std::string trustpath_;

  std::vector<char> certBuffer_;
  std::vector<char> keyBuffer_;
  std::vector<std::vector<char>> trustBuffers_;
};

}  // namespace org::apache::nifi::minifi::processors
