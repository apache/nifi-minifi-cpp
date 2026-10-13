// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#pragma once

#include <expected>
#include <string>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "OPCCommon.h"
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
  EXTENSIONAPI static constexpr auto Properties = std::to_array<core::PropertyReference>({
      OPCServerEndPoint,
      ApplicationURI,
      Username,
      Password,
      CertificatePath,
      KeyPath,
      TrustedPath
  });

  using ProcessorImpl::ProcessorImpl;

  void onSchedule(core::ProcessContext& context, core::ProcessSessionFactory& factory) override;

 protected:
  struct TargetNodeProperties {
    core::PropertyReference id_type;
    core::PropertyReference id;
    core::PropertyReference namespace_index;
    core::PropertyReference path_reference_types;
  };

  struct TargetNode {
    opc::NodeId node_id;
    bool exists = false;
  };

  virtual bool reconnect();
  void readPathReferenceTypes(core::ProcessContext& context, const core::PropertyReference& path_reference_types_property, const std::string& node_id);
  void parseNode(core::ProcessContext& context, const core::PropertyReference& path_reference_types_property);
  std::expected<const std::vector<opc::NodeId>*, std::string> resolvePathNodeIds();
  std::expected<const opc::NodeId*, std::string> resolveNodeId();
  std::expected<TargetNode, std::string> resolveTargetNode(core::ProcessContext& context, const core::FlowFile& flow_file,
      const TargetNodeProperties& properties) const;

  std::string node_id_;
  UA_UInt16 namespace_idx_ = 0;
  opc::NodeId node_;  // Only used for non-path node id types
  // Cached translation of the configured path, cleared on (re)connect, as a restarted server may reassign node ids
  std::optional<std::vector<opc::NodeId>> resolved_path_node_ids_;
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

 private:
  std::expected<opc::NodeId, std::string> resolveSingleNodePath(const std::string& path, UA_UInt16 namespace_idx,
      const std::vector<UA_UInt32>& reference_types) const;
  std::expected<std::vector<opc::NodeId>, std::string> translatePath(const std::string& path, UA_UInt16 namespace_idx,
      const std::vector<UA_UInt32>& reference_types) const;
  static std::expected<void, std::string> requireSingleNode(const std::string& path, const std::vector<opc::NodeId>& node_ids);
};

}  // namespace org::apache::nifi::minifi::processors
