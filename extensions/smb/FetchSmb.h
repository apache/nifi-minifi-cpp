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
#include <regex>
#include <string>
#include <utility>

#include "SmbConnectionControllerService.h"
#include "core/Processor.h"
#include "core/ProcessSession.h"
#include "core/Property.h"
#include "core/PropertyDefinition.h"
#include "core/PropertyDefinitionBuilder.h"
#include "core/OutputAttributeDefinition.h"
#include "core/logging/LoggerFactory.h"
#include "utils/Enum.h"
#include "utils/ListingStateManager.h"
#include "utils/file/ListedFile.h"
#include "utils/file/FileUtils.h"

namespace org::apache::nifi::minifi::extensions::smb {

class FetchSmb : public core::ProcessorImpl {
 public:
  explicit FetchSmb(std::string name, const utils::Identifier& uuid = {})
      : core::ProcessorImpl(std::move(name), uuid) {
  }

  EXTENSIONAPI static constexpr const char* Description = "Fetches files from a SMB Share. Designed to be used in tandem with ListSmb.";

  EXTENSIONAPI static constexpr auto ConnectionControllerService = core::PropertyDefinitionBuilder<>::createProperty("SMB Connection Controller Service")
      .withDescription("Specifies the SMB connection controller service to use for connecting to the SMB server. "
                       "If the SMB share is auto-mounted to a drive letter, its recommended to use FetchFile instead.")
      .isRequired(true)
      .withAllowedTypes<SmbConnectionControllerService>()
      .build();
  EXTENSIONAPI static constexpr auto RemoteFile = core::PropertyDefinitionBuilder<>::createProperty("Input Directory")
      .withDescription("The full path of the file to be retrieved from the remote server. If left empty, the path and filename attributes of the incoming flow file will be used.")
      .isRequired(false)
      .supportsExpressionLanguage(true)
      .build();
  EXTENSIONAPI static constexpr auto Properties = std::to_array<core::PropertyReference>({
      ConnectionControllerService,
      RemoteFile
  });

  EXTENSIONAPI static constexpr auto Success = core::RelationshipDefinition{"success", "A flowfile will be routed here for each successfully fetched file."};
  EXTENSIONAPI static constexpr auto Failure = core::RelationshipDefinition{"failure", "A flowfile will be routed here when failed to fetch its content."};
  EXTENSIONAPI static constexpr auto Relationships = std::array{Success, Failure};

  EXTENSIONAPI static constexpr auto ErrorCode = core::OutputAttributeDefinition<>{"error.code", { Failure }, "The error code returned by SMB when the fetch of a file fails."};
  EXTENSIONAPI static constexpr auto ErrorMessage = core::OutputAttributeDefinition<>{"error.message", { Failure }, "The error message returned by SMB when the fetch of a file fails."};

  EXTENSIONAPI static constexpr auto OutputAttributes = std::array<core::OutputAttributeReference, 2>{ FetchSmb::ErrorCode, ErrorMessage };

  EXTENSIONAPI static constexpr bool SupportsDynamicProperties = false;
  EXTENSIONAPI static constexpr bool SupportsDynamicRelationships = false;
  EXTENSIONAPI static constexpr core::annotation::Input InputRequirement = core::annotation::Input::INPUT_REQUIRED;
  EXTENSIONAPI static constexpr bool IsSingleThreaded = false;

  ADD_COMMON_VIRTUAL_FUNCTIONS_FOR_PROCESSORS

  void initialize() override;
  void onSchedule(core::ProcessContext& context, core::ProcessSessionFactory& session_factory) override;
  void onTrigger(core::ProcessContext& context, core::ProcessSession& session) override;

 private:
  std::shared_ptr<SmbConnectionControllerService> smb_connection_controller_service_;
  std::shared_ptr<core::logging::Logger> logger_ = core::logging::LoggerFactory<FetchSmb>::getLogger(uuid_);
};

}  // namespace org::apache::nifi::minifi::extensions::smb
