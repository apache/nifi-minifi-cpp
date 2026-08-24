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

#include "ForkEnrichment.h"

#include "api/core/ProcessSession.h"
#include "api/utils/ProcessorConfigUtils.h"
#include "stduuid/uuid.hpp"

namespace org::apache::nifi::minifi::enrichment {

minifi_status ForkEnrichment::onScheduleImpl(api::core::ProcessContext& context) {
  max_batch_size_ = api::utils::parseOptionalU64Property(context, MaxBatchSize);
  if (max_batch_size_ && *max_batch_size_ == 0) {
    return MINIFI_STATUS_VALIDATION_FAILED;
  }

  return MINIFI_STATUS_SUCCESS;
}

minifi_status ForkEnrichment::onTriggerImpl(api::core::ProcessContext&, api::core::ProcessSession& session) {
  uint64_t processed = 0;
  while (api::core::FlowFile original = session.get()) {
    api::core::FlowFile enrichment = session.clone(original);

    session.setAttribute(original, ENRICHMENT_ROLE, "ORIGINAL");
    session.setAttribute(enrichment, ENRICHMENT_ROLE, "ENRICHMENT");

    const std::string group_id = uuids::to_string(uuid_gen());
    session.setAttribute(original, ENRICHMENT_GROUP_ID, group_id);
    session.setAttribute(enrichment, ENRICHMENT_GROUP_ID, group_id);

    session.transfer(std::move(original), Original);
    session.transfer(std::move(enrichment), Enrichment);
    if (max_batch_size_ && ++processed >= max_batch_size_) {
      break;
    }
  }
  return MINIFI_STATUS_SUCCESS;
}

}  // namespace org::apache::nifi::minifi::enrichment
