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

#include "core/Resource.h"
#include "minifi-cpp/core/ProcessSession.h"
#include "utils/ProcessorConfigUtils.h"

namespace org::apache::nifi::minifi::standard {
void ForkEnrichment::initialize() {
  setSupportedProperties(Properties);
  setSupportedRelationships(Relationships);
  ProcessorImpl::initialize();
}

void ForkEnrichment::onSchedule(core::ProcessContext& context, core::ProcessSessionFactory& session_factory) {
  max_batch_size_ = utils::parseOptionalU64Property(context, MaxBatchSize);
  if (max_batch_size_ && *max_batch_size_ == 0) {
    throw Exception(PROCESSOR_EXCEPTION, "Max Batch Size property is invalid");
  }

  ProcessorImpl::onSchedule(context, session_factory);
}

void ForkEnrichment::onTrigger(core::ProcessContext&, core::ProcessSession& session) {
  uint64_t processed = 0;
  while (const auto original = session.get()) {
    const auto enrichment = session.clone(*original);

    original->setAttribute(ENRICHMENT_ROLE, "ORIGINAL");
    enrichment->setAttribute(ENRICHMENT_ROLE, "ENRICHMENT");

    const std::string group_id = utils::IdGenerator::getIdGenerator()->generate().to_string();
    original->setAttribute(ENRICHMENT_GROUP_ID, group_id);
    enrichment->setAttribute(ENRICHMENT_GROUP_ID, group_id);

    session.transfer(original, Original);
    session.transfer(enrichment, Enrichment);
    if (max_batch_size_ && ++processed >= max_batch_size_) {
      break;
    }
  }
}

REGISTER_RESOURCE(ForkEnrichment, Processor);
}  // namespace org::apache::nifi::minifi::standard
