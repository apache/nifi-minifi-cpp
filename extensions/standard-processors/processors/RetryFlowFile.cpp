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

#include "RetryFlowFile.h"

#include "utils/ProcessorConfigUtils.h"
#include "core/Resource.h"

namespace org::apache::nifi::minifi::processors {

void RetryFlowFile::initialize() {
  setSupportedProperties(Properties);
  setSupportedRelationships(Relationships);
}

void RetryFlowFile::onSchedule(core::ProcessContext& context, core::ProcessSessionFactory&) {
  retry_attribute_ = utils::parseProperty(context, RetryAttribute);
  maximum_retries_ = utils::parseU64Property(context, MaximumRetries);
  penalize_retries_ = utils::parseBoolProperty(context, PenalizeRetries);
  fail_on_non_numerical_overwrite_ = utils::parseBoolProperty(context, FailOnNonNumericalOverwrite);
  reuse_mode_ = utils::parseProperty(context, ReuseMode);
  readDynamicPropertyKeys(context);
}

void RetryFlowFile::onTrigger(core::ProcessContext& context, core::ProcessSession& session) {
  auto flow_file = session.get();
  if (!flow_file) {
    return;
  }

  const auto maybe_retry_property_value = getRetryPropertyValue(flow_file);
  if (!maybe_retry_property_value) {
    session.transfer(flow_file, Failure);
    return;
  }
  uint64_t retry_property_value = maybe_retry_property_value.value();
  const std::string last_retried_by_property_name = retry_attribute_ + ".uuid";
  const std::string current_processor_uuid = getUUIDStr();
  std::string last_retried_by_uuid;
  if (flow_file->getAttribute(last_retried_by_property_name, last_retried_by_uuid)) {
    if (last_retried_by_uuid != current_processor_uuid) {
      if (reuse_mode_ == FAIL_ON_REUSE) {
        logger_->log_error("FlowFile {} was previously retried with the same attribute by a different "
            "processor (uuid: {}, current uuid: {}). Transfering flowfile to 'failure'...",
            flow_file->getUUIDStr(), last_retried_by_uuid, current_processor_uuid);
        session.transfer(flow_file, Failure);
        return;
      }
      // Assuming reuse_mode_ == WARN_ON_REUSE || reuse_mode_ == RESET_REUSE
      core::logging::LOG_LEVEL reuse_mode_log_level(core::logging::LOG_LEVEL::debug);
      if (reuse_mode_ == WARN_ON_REUSE) {
        reuse_mode_log_level = core::logging::LOG_LEVEL::warn;
      }
      logger_->log_string(reuse_mode_log_level, "Reusing retry attribute that belongs to different processor. Resetting value to 0.");
      retry_property_value = 0;
    }
  }

  if (retry_property_value < maximum_retries_) {
    flow_file->setAttribute(retry_attribute_, std::to_string(retry_property_value + 1));
    if (penalize_retries_) {
      session.penalize(flow_file);
    }
    session.transfer(flow_file, Retry);
    return;
  }
  setRetriesExceededAttributesOnFlowFile(context, flow_file);
  session.transfer(flow_file, RetriesExceeded);
}

void RetryFlowFile::readDynamicPropertyKeys(const core::ProcessContext& context) {
  exceeded_flowfile_attribute_keys_.clear();
  const std::vector<std::string> dynamic_prop_keys = context.getDynamicPropertyKeys();
  logger_->log_info("RetryFlowFile registering {} keys", dynamic_prop_keys.size());
  for (const auto& key : dynamic_prop_keys) {
    exceeded_flowfile_attribute_keys_.emplace_back(core::PropertyDefinitionBuilder<>::createProperty(key).withDescription("auto generated").supportsExpressionLanguage(true).build());
    logger_->log_info("RetryFlowFile registered attribute '{}'", key);
  }
}

std::optional<uint64_t> RetryFlowFile::getRetryPropertyValue(const std::shared_ptr<core::FlowFile>& flow_file) const {
  std::string value_as_string;
  flow_file->getAttribute(retry_attribute_, value_as_string);
  if (const auto value = parsing::parseIntegral<uint64_t>(value_as_string)) {
    return *value;
  }
  if (fail_on_non_numerical_overwrite_) {
    logger_->log_info("Non-numerical retry property in RetryFlowFile (value: {}). Sending flowfile to failure...", value_as_string);
    return std::nullopt;
  }
  logger_->log_info("Non-numerical retry property in RetryFlowFile: overwriting {} with 0.", value_as_string);
  return 0;
}

void RetryFlowFile::setRetriesExceededAttributesOnFlowFile(const core::ProcessContext& context, const std::shared_ptr<core::FlowFile>& flow_file) const {
  for (const auto& [key, value] : context.getDynamicProperties(flow_file.get())) {
    flow_file->setAttribute(key, value);
    logger_->log_info("Set attribute '{}' of flow file '{}' with value '{}'", key, flow_file->getUUIDStr(), value);
  }
}

REGISTER_RESOURCE(RetryFlowFile, Processor);

}  // namespace org::apache::nifi::minifi::processors
