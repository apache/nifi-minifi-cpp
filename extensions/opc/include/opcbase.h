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
#include "core/ProcessorImpl.h"
#include "core/ProcessSession.h"
#include "minifi-cpp/core/PropertyDefinition.h"
#include "core/PropertyDefinitionBuilder.h"
#include "core/Core.h"
#include "minifi-cpp/core/Property.h"

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
      .withDescription("Comma separated list of paths to the DER-encoded trusted server certificates")
      .build();
  EXTENSIONAPI static constexpr auto PathReferenceTypes = core::PropertyDefinitionBuilder<>::createProperty("Path reference types")
      .withDescription("Specify the reference types between nodes in the path if Path Node ID type is used. If not provided, all reference types are assumed to be Organizes. "
                       "The format is 'referenceType1/referenceType2/.../referenceTypeN' and the supported reference types are Organizes, HasComponent, HasProperty, and HasSubtype.")
      .build();
  EXTENSIONAPI static constexpr auto Properties = std::to_array<core::PropertyReference>({
      OPCServerEndPoint,
      ApplicationURI,
      Username,
      Password,
      CertificatePath,
      KeyPath,
      TrustedPath,
      PathReferenceTypes
  });

  using ProcessorImpl::ProcessorImpl;

  void onSchedule(core::ProcessContext& context, core::ProcessSessionFactory& factory) override;

 protected:
  virtual bool reconnect();
  void readPathReferenceTypes(core::ProcessContext& context, const std::string& node_id);
  void parseIdType(core::ProcessContext& context, const core::PropertyReference& prop);

  std::string node_id_;
  int32_t namespace_idx_ = 0;
  opc::OPCNodeIDType id_type_{};

  opc::ClientPtr connection_;

  std::string endpoint_url_;

  std::string application_uri_;
  std::string username_;
  std::string password_;
  std::string certpath_;
  std::string keypath_;
  std::string trustpath_;

  std::vector<char> cert_buffer_;
  std::vector<char> key_buffer_;
  std::vector<std::vector<char>> trust_buffers_;
  std::vector<UA_UInt32> path_reference_types_;
};

}  // namespace org::apache::nifi::minifi::processors
