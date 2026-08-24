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

#include "EnrichmentUtils.h"
#include "api/core/Resource.h"
#include "api/utils/ProcessorConfigUtils.h"
#include "utils/AttributeErrors.h"

namespace org::apache::nifi::minifi::enrichment {

minifi_status JoinEnrichmentAttributes::onScheduleImpl(api::core::ProcessContext& context) {
  using namespace std::literals::chrono_literals;
  if (const auto timeout = api::utils::parseOptionalDurationProperty(context, TimeoutProperty); timeout && *timeout > 0ms) {
    time_out_tracker_.emplace(*timeout);
  }
  max_batch_size_ = api::utils::parseOptionalU64Property(context, MaxBatchSize);
  if (max_batch_size_ && *max_batch_size_ == 0) {
    return MINIFI_STATUS_VALIDATION_FAILED;
  }
  return MINIFI_STATUS_SUCCESS;
}

namespace {
bool checkRequiredAttributes(api::core::ProcessSession& session, api::core::FlowFile& flow_file) {
  return session.getAttribute(flow_file, ENRICHMENT_ROLE).has_value() && session.getAttribute(flow_file, ENRICHMENT_GROUP_ID).has_value();
}
}  // namespace

void JoinEnrichmentAttributes::join(api::core::FlowFile& original, api::core::FlowFile& enrichment, api::core::ProcessSession& session) {
  api::core::FlowFile joined = session.clone(original);
  for (const auto& [k, v] : session.getAttributes(enrichment)) {
    if (k != ENRICHMENT_ROLE) {
      session.setAttribute(joined, k, v);
    }
  }
  session.setAttribute(joined, ENRICHMENT_ROLE, "JOINED");
  session.transfer(std::move(original), Original);
  session.transfer(std::move(enrichment), Original);
  session.transfer(std::move(joined), Joined);
}

void JoinEnrichmentAttributes::handleFlowFile(api::core::FlowFile flow_file, api::core::ProcessSession& session,
    const std::chrono::steady_clock::time_point current_time) {
  if (!checkRequiredAttributes(session, flow_file)) {
    logger_->log_warn("{} is missing enrichment.group.id and/or enrichment.role, routing it to Invalid", session.getFlowFileId(flow_file));
    session.transfer(std::move(flow_file), Invalid);
    return;
  }

  // SAFETY: checkRequiredAttributes already checks for ENRICHMENT_ROLE
  const auto role = parsing::parseEnum<EnrichmentRole>(*session.getAttribute(flow_file, ENRICHMENT_ROLE));
  if (!role) {
    logger_->log_warn("{} has invalid role due to {}", session.getFlowFileId(flow_file), role.error());
    session.transfer(std::move(flow_file), Invalid);
    return;
  }

  // SAFETY: checkRequiredAttributes already checks for ENRICHMENT_GROUP_ID
  const std::string group_id = *session.getAttribute(flow_file, ENRICHMENT_GROUP_ID);

  auto& my_map = role == EnrichmentRole::ENRICHMENT ? enrichments_ : originals_;
  auto& pair_map = role == EnrichmentRole::ENRICHMENT ? originals_ : enrichments_;

  if (const auto previous_node = my_map.extract(group_id)) {
    logger_->log_warn("Encountered duplicate {} for {}, routing both to Invalid", magic_enum::enum_name(*role), group_id);
    session.transfer(std::move(flow_file), Invalid);
    session.transfer(session.unstash(std::move(previous_node.mapped())), Invalid);
    return;
  }

  if (const auto pair_node = pair_map.extract(group_id)) {
    logger_->log_trace("Match found for {}", group_id);
    api::core::FlowFile pair = session.unstash(std::move(pair_node.mapped()));
    auto [original, enrichment] =
        (*role == EnrichmentRole::ORIGINAL) ? std::tie(flow_file, pair) : std::tie(pair, flow_file);
    join(original, enrichment, session);
  } else {
    my_map.insert({group_id, session.stash(std::move(flow_file))});
    if (time_out_tracker_) {
      time_out_tracker_->track(std::move(group_id), current_time);
    }
  }
}

minifi_status JoinEnrichmentAttributes::onTriggerImpl(api::core::ProcessContext&, api::core::ProcessSession& session) {
  const auto current_time = std::chrono::steady_clock::now();
  uint64_t processed = 0;
  while (auto flow_file = session.get()) {
    handleFlowFile(std::move(flow_file), session, current_time);
    if (max_batch_size_ && ++processed >= max_batch_size_) {
      break;
    }
  }

  if (time_out_tracker_) {
    for (auto timed_out_group : time_out_tracker_->getTimedOutFlowFiles(current_time)) {
      const auto removeFromMap = [&](StoredFlowFileMap& map) {
        if (auto timed_out_node = map.extract(timed_out_group)) {
          session.transfer(session.unstash(std::move(timed_out_node.mapped())), TimeoutRelationship);
        }
      };
      removeFromMap(originals_);
      removeFromMap(enrichments_);
    }
  }
  return MINIFI_STATUS_SUCCESS;
}

}  // namespace org::apache::nifi::minifi::enrichment
