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
#include <string>
#include <utility>

#include "../GCPAttributes.h"
#include "GCSProcessor.h"
#include "core/logging/LoggerConfiguration.h"
#include "core/OutputAttributeDefinition.h"
#include "core/PropertyDefinition.h"
#include "core/PropertyDefinitionBuilder.h"
#include "utils/ArrayUtils.h"

namespace org::apache::nifi::minifi::extensions::gcp {

class DeleteGCSObject : public GCSProcessor {
 public:
  explicit DeleteGCSObject(std::string_view name, const utils::Identifier& uuid = {})
      : GCSProcessor(name, uuid, core::logging::LoggerFactory<DeleteGCSObject>::getLogger(uuid)) {
  }
  ~DeleteGCSObject() override = default;

  EXTENSIONAPI static constexpr const char* Description = "Deletes an object from a Google Cloud Bucket.";

  EXTENSIONAPI static constexpr auto Bucket = core::PropertyDefinitionBuilder<>::createProperty("Bucket")
        .withDescription("Bucket of the object.")
        .withDefaultValue("${gcs.bucket}")
        .supportsExpressionLanguage(true)
        .build();
  EXTENSIONAPI static constexpr auto Key = core::PropertyDefinitionBuilder<>::createProperty("Key")
        .withDescription("Name of the object.")
        .withDefaultValue("${filename}")
        .supportsExpressionLanguage(true)
        .build();
  EXTENSIONAPI static constexpr auto EncryptionKey = core::PropertyDefinitionBuilder<>::createProperty("Server Side Encryption Key")
        .withDescription("The AES256 Encryption Key (encoded in base64) for server-side decryption of the object.")
        .isRequired(false)
        .supportsExpressionLanguage(true)
        .build();
  EXTENSIONAPI static constexpr auto ObjectGeneration = core::PropertyDefinitionBuilder<>::createProperty("Object Generation")
        .withDescription("The generation of the Object to download. If left empty, then it will download the latest generation.")
        .supportsExpressionLanguage(true)
        .build();
  EXTENSIONAPI static constexpr auto Properties = utils::array_cat(GCSProcessor::Properties, std::array<core::PropertyReference, 4>{
      Bucket,
      Key,
      EncryptionKey,
      ObjectGeneration
  });


  EXTENSIONAPI static constexpr auto Success = core::RelationshipDefinition{"success", "FlowFiles are routed to this relationship after a successful Google Cloud Storage operation."};
  EXTENSIONAPI static constexpr auto Failure = core::RelationshipDefinition{"failure", "FlowFiles are routed to this relationship if the Google Cloud Storage operation fails."};
  EXTENSIONAPI static constexpr auto Relationships = std::array{Success, Failure};

  EXTENSIONAPI static constexpr auto Message = core::OutputAttributeDefinition<>{GCS_STATUS_MESSAGE, { Failure }, "The status message received from google cloud."};
  EXTENSIONAPI static constexpr auto Reason = core::OutputAttributeDefinition<>{GCS_ERROR_REASON, { Failure }, "The description of the error occurred during operation."};
  EXTENSIONAPI static constexpr auto Domain = core::OutputAttributeDefinition<>{GCS_ERROR_DOMAIN, { Failure }, "The domain of the error occurred during operation."};
  EXTENSIONAPI static constexpr auto OutputAttributes = std::array<core::OutputAttributeReference, 3>{Message, Reason, Domain};

  EXTENSIONAPI static constexpr bool SupportsDynamicProperties = false;
  EXTENSIONAPI static constexpr bool SupportsDynamicRelationships = false;
  EXTENSIONAPI static constexpr core::annotation::Input InputRequirement = core::annotation::Input::INPUT_REQUIRED;
  EXTENSIONAPI static constexpr bool IsSingleThreaded = false;

  ADD_COMMON_VIRTUAL_FUNCTIONS_FOR_PROCESSORS

  void initialize() override;
  void onTrigger(const std::shared_ptr<core::ProcessContext>& context, const std::shared_ptr<core::ProcessSession>& session) override;
};

}  // namespace org::apache::nifi::minifi::extensions::gcp
