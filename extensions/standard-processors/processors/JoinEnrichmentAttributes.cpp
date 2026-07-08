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
#include "JoinEnrichmentAttributes.h"

#include "core/Resource.h"
#include "minifi-cpp/core/ProcessSession.h"
#include "utils/AttributeErrors.h"
#include "utils/EnrichmentUtils.h"
#include "utils/ProcessorConfigUtils.h"

namespace org::apache::nifi::minifi::standard {
const core::Relationship JoinEnrichmentAttributes::Self("__self__", "Marks the FlowFile to be owned by this processor");

void JoinEnrichmentAttributes::initialize() {
  setSupportedProperties(Properties);
  setSupportedRelationships(Relationships);
  ProcessorImpl::initialize();
}

void JoinEnrichmentAttributes::onSchedule(core::ProcessContext& context, core::ProcessSessionFactory& session_factory) {
  using namespace std::literals::chrono_literals;
  if (const auto timeout = utils::parseOptionalDurationProperty(context, TimeoutProperty); timeout && *timeout > 0ms) {
    time_out_tracker_.emplace(*timeout);
  }
  max_batch_size_ = utils::parseOptionalU64Property(context, MaxBatchSize);
  if (max_batch_size_ && *max_batch_size_ == 0) {
    throw Exception(PROCESSOR_EXCEPTION, "Max Batch Size property is invalid");
  }
  ProcessorImpl::onSchedule(context, session_factory);
}

namespace {
bool checkRequiredAttributes(const core::FlowFile& flow_file) {
  return flow_file.getAttribute(ENRICHMENT_ROLE).has_value() && flow_file.getAttribute(ENRICHMENT_GROUP_ID).has_value();
}
}  // namespace

void JoinEnrichmentAttributes::join(const std::shared_ptr<core::FlowFile>& original, const std::shared_ptr<core::FlowFile>& enrichment,
    core::ProcessSession& session) const {
  const auto cloned = session.clone(*original);
  for (const auto& [k, v] : enrichment->getAttributes()) {
    if (k != ENRICHMENT_ROLE) {
      cloned->setAttribute(k, v);
    }
  }
  cloned->setAttribute(ENRICHMENT_ROLE, "JOINED");
  if (!std::ranges::contains(session_flow_files_, original->getUUID())) {
    session.add(original);
  }
  if (!std::ranges::contains(session_flow_files_, enrichment->getUUID())) {
    session.add(enrichment);
  }
  session.transfer(original, Original);
  session.transfer(enrichment, Original);
  session.transfer(cloned, Joined);
}

void JoinEnrichmentAttributes::handleFlowFile(std::shared_ptr<core::FlowFile> flow_file, core::ProcessSession& session,
    const std::chrono::steady_clock::time_point current_time) {
  if (!checkRequiredAttributes(*flow_file)) {
    logger_->log_warn("{} is missing enrichment.group.id and/or enrichment.role, routing it to Invalid", flow_file->getId());
    session.transfer(flow_file, Invalid);
    return;
  }

  const auto role = flow_file->getAttribute(ENRICHMENT_ROLE) | utils::toExpected(make_error_code(core::AttributeErrorCode::MissingAttribute)) |
      utils::andThen(parsing::parseEnum<EnrichmentRole>);
  if (!role) {
    logger_->log_warn("{} has invalid role due to {}", flow_file->getId(), role.error());
    session.transfer(flow_file, Invalid);
    return;
  }

  std::string group_id = *(flow_file->getAttribute(ENRICHMENT_GROUP_ID));

  auto& my_map = role == EnrichmentRole::ENRICHMENT ? enrichments_ : originals_;
  auto& pair_map = role == EnrichmentRole::ENRICHMENT ? originals_ : enrichments_;

  if (const auto previous_node = my_map.extract(group_id)) {
    logger_->log_warn("Encountered duplicate {} for {}, routing both to Invalid", magic_enum::enum_name(*role), group_id);
    session.transfer(flow_file, Invalid);
    session.transfer(previous_node.mapped(), Invalid);
    if (!std::ranges::contains(session_flow_files_, previous_node.mapped()->getUUID())) {
      session.add(previous_node.mapped());
    }
    return;
  }

  if (const auto pair_node = pair_map.extract(group_id)) {
    logger_->log_trace("Match found");
    auto [original,
        enrichment] = (*role == EnrichmentRole::ORIGINAL) ? std::tie(flow_file, pair_node.mapped()) : std::tie(pair_node.mapped(), flow_file);
    join(original, enrichment, session);
  } else {
    logger_->log_trace("Missing other half routing {} to Self", flow_file->getId());
    my_map.insert({group_id, flow_file});
    session.transfer(flow_file, Self);
    if (time_out_tracker_) {
      time_out_tracker_->track(std::move(group_id), current_time);
    }
  }
}

void JoinEnrichmentAttributes::onTrigger(core::ProcessContext&, core::ProcessSession& session) {
  const auto current_time = std::chrono::steady_clock::now();
  for (auto flow_file : flow_file_store_.getNewFlowFiles()) {
    handleFlowFile(std::move(flow_file), session, current_time);
  }
  uint64_t processed = 0;
  while (auto flow_file = session.get()) {
    session_flow_files_.push_back(flow_file->getUUID());
    handleFlowFile(std::move(flow_file), session, current_time);
    if (max_batch_size_ && ++processed >= max_batch_size_) {
      break;
    }
  }

  if (time_out_tracker_) {
    for (auto timed_out_group : time_out_tracker_->getTimedOutFlowFiles(current_time)) {
      const auto removeFromMap = [&](MapType& map) {
        if (auto timed_out_node = map.extract(timed_out_group)) {
          // Fresh FlowFiles shouldn't time out (we only use a single time_point per session)
          gsl_AssertAudit(!std::ranges::contains(session_flow_files_, timed_out_node.mapped()->getUUID()));

          session.add(timed_out_node.mapped());
          session.transfer(timed_out_node.mapped(), TimeoutRelationship);
        }
      };
      removeFromMap(originals_);
      removeFromMap(enrichments_);
    }
  }

  session_flow_files_.clear();
}

void JoinEnrichmentAttributes::restore(const std::shared_ptr<core::FlowFile>& flowFile) {
  if (!flowFile) {
    return;
  }
  flow_file_store_.put(flowFile);
}

REGISTER_RESOURCE(JoinEnrichmentAttributes, Processor);

}  // namespace org::apache::nifi::minifi::standard
