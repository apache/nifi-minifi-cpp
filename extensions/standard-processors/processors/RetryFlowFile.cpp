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

#include "core/Resource.h"

namespace org::apache::nifi::minifi::processors {

void RetryFlowFile::initialize() {
  setSupportedProperties(Properties);
  setSupportedRelationships(Relationships);
}

void RetryFlowFile::onSchedule(core::ProcessContext* context, core::ProcessSessionFactory* /* sessionFactory */) {
  context->getProperty(RetryAttribute, retry_attribute_);
  context->getProperty(MaximumRetries, maximum_retries_);
  context->getProperty(PenalizeRetries, penalize_retries_);
  context->getProperty(FailOnNonNumericalOverwrite, fail_on_non_numerical_overwrite_);
  context->getProperty(ReuseMode, reuse_mode_);
  readDynamicPropertyKeys(context);
}

void RetryFlowFile::onTrigger(core::ProcessContext* context, core::ProcessSession* session) {
  auto flow_file = session->get();
  if (!flow_file) {
    return;
  }

  const auto maybe_retry_property_value = getRetryPropertyValue(flow_file);
  if (!maybe_retry_property_value) {
    session->transfer(flow_file, Failure);
    return;
  }
  uint64_t retry_property_value = maybe_retry_property_value.value();
  const std::string last_retried_by_property_name = retry_attribute_ + ".uuid";
  const std::string current_processor_uuid = getUUIDStr();
  std::string last_retried_by_uuid;
  if (flow_file->getAttribute(last_retried_by_property_name, last_retried_by_uuid)) {
    if (last_retried_by_uuid != current_processor_uuid) {
      if (reuse_mode_ == FAIL_ON_REUSE) {
        logger_->log_error("FlowFile %s was previously retried with the same attribute by a different "
            "processor (uuid: %s, current uuid: %s). Transfering flowfile to 'failure'...",
            flow_file->getUUIDStr(), last_retried_by_uuid, current_processor_uuid);
        session->transfer(flow_file, Failure);
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
      session->penalize(flow_file);
    }
    session->transfer(flow_file, Retry);
    return;
  }
  setRetriesExceededAttributesOnFlowFile(context, flow_file);
  session->transfer(flow_file, RetriesExceeded);
}

void RetryFlowFile::readDynamicPropertyKeys(core::ProcessContext* context) {
  exceeded_flowfile_attribute_keys_.clear();
  const std::vector<std::string> dynamic_prop_keys = context->getDynamicPropertyKeys();
  logger_->log_info("RetryFlowFile registering %d keys", dynamic_prop_keys.size());
  for (const auto& key : dynamic_prop_keys) {
    exceeded_flowfile_attribute_keys_.emplace_back(core::PropertyDefinitionBuilder<>::createProperty(key).withDescription("auto generated").supportsExpressionLanguage(true).build());
    logger_->log_info("RetryFlowFile registered attribute '%s'", key);
  }
}

std::optional<uint64_t> RetryFlowFile::getRetryPropertyValue(const std::shared_ptr<core::FlowFile>& flow_file) const {
  std::string value_as_string;
  flow_file->getAttribute(retry_attribute_, value_as_string);
  uint64_t value;
  try {
    utils::internal::ValueParser(value_as_string).parse(value).parseEnd();
    return value;
  }
  catch(const utils::internal::ParseException&) {
    if (fail_on_non_numerical_overwrite_) {
      logger_->log_info("Non-numerical retry property in RetryFlowFile (value: %s). Sending flowfile to failure...", value_as_string);
      return std::nullopt;
    }
  }
  logger_->log_info("Non-numerical retry property in RetryFlowFile: overwriting %s with 0.", value_as_string);
  return 0;
}

void RetryFlowFile::setRetriesExceededAttributesOnFlowFile(core::ProcessContext* context, const std::shared_ptr<core::FlowFile>& flow_file) const {
  for (const auto& attribute : exceeded_flowfile_attribute_keys_) {
    std::string value;
    context->getDynamicProperty(attribute, value, flow_file);
    flow_file->setAttribute(attribute.getName(), value);
    logger_->log_info("Set attribute '%s' of flow file '%s' with value '%s'", attribute.getName(), flow_file->getUUIDStr(), value);
  }
}

REGISTER_RESOURCE(RetryFlowFile, Processor);

}  // namespace org::apache::nifi::minifi::processors
