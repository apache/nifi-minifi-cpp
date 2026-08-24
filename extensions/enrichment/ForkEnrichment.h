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

#include <random>

#include "EnrichmentUtils.h"
#include "api/core/ProcessorImpl.h"
#include "api/utils/Export.h"
#include "core/PropertyDefinitionBuilder.h"
#include "minifi-cpp/core/Annotation.h"
#include "minifi-cpp/core/PropertyDefinition.h"
#include "stduuid/uuid.hpp"

namespace org::apache::nifi::minifi::enrichment {

class ForkEnrichment : public api::core::ProcessorImpl {
 public:
  using ProcessorImpl::ProcessorImpl;

  EXTENSIONAPI static constexpr const char* Description =
      "Used in conjunction with the JoinEnrichmentAttributes processor, this processor is responsible for adding the attributes that are necessary "
      "for the JoinEnrichmentAttributes processor to perform its function. Each incoming FlowFile will be cloned. The original FlowFile will have "
      "appropriate attributes added and then be transferred to the 'original' relationship. The clone will have appropriate attributes added and "
      "then be routed to the 'enrichment' relationship.";

  EXTENSIONAPI static constexpr auto MaxBatchSize =
      core::PropertyDefinitionBuilder<>::createProperty("Max Batch Size")
          .withDescription("The maximum number of flow files to process at a time. If unset, all FlowFiles will be processed at once.")
          .withValidator(core::StandardPropertyValidators::UNSIGNED_INTEGER_VALIDATOR)
          .build();

  EXTENSIONAPI static constexpr auto Enrichment = core::RelationshipDefinition{"enrichment",
      "A clone of the incoming FlowFile will be routed to this relationship, after adding appropriate attributes."};
  EXTENSIONAPI static constexpr auto Original = core::RelationshipDefinition{"original",
      "The incoming FlowFile will be routed to this relationship, after adding appropriate attributes."};
  EXTENSIONAPI static constexpr auto Properties = std::array<core::PropertyReference, 1>{MaxBatchSize};
  EXTENSIONAPI static constexpr auto Relationships = std::array{Enrichment, Original};

  EXTENSIONAPI static constexpr bool SupportsDynamicProperties = false;
  EXTENSIONAPI static constexpr bool SupportsDynamicRelationships = false;
  EXTENSIONAPI static constexpr core::annotation::Input InputRequirement = core::annotation::Input::INPUT_REQUIRED;
  EXTENSIONAPI static constexpr bool IsSingleThreaded = false;

  EXTENSIONAPI static constexpr auto EnrichmentRole = core::OutputAttributeDefinition<2>{
      ENRICHMENT_ROLE, {Enrichment, Original}, "The role to use for enrichment. This will either be ORIGINAL or ENRICHMENT."};
  EXTENSIONAPI static constexpr auto EnrichmentGroupId = core::OutputAttributeDefinition<2>{ENRICHMENT_GROUP_ID,
      {Enrichment, Original},
      "The Group ID to use in order to correlate the 'original' FlowFile with the 'enrichment' FlowFile."};

  EXTENSIONAPI static constexpr auto OutputAttributes = std::array<core::OutputAttributeReference, 2>{EnrichmentRole, EnrichmentGroupId};

 protected:
  minifi_status onScheduleImpl(api::core::ProcessContext& context) override;
  minifi_status onTriggerImpl(api::core::ProcessContext& context, api::core::ProcessSession& session) override;

 private:
  std::optional<uint64_t> max_batch_size_;
  std::random_device rd;
  std::mt19937 rng{rd()};
  uuids::uuid_random_generator uuid_gen{rng};
};
}  // namespace org::apache::nifi::minifi::enrichment
