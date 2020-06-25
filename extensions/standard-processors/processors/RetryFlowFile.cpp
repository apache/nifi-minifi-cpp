/**
 *
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

#include "core/PropertyValidation.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace processors {

core::Property RetryFlowFile::RetryAttribute(core::PropertyBuilder::createProperty("Retry Attribute")
    ->withDescription(
        "The name of the attribute that contains the current retry count for the FlowFile."
        "WARNING: If the name matches an attribute already on the FlowFile that does not contain a numerical value, "
        "the processor will either overwrite that attribute with '1' or fail based on configuration.")
    ->withDefaultValue("flowfile.retries")
    ->supportsExpressionLanguage(true)
    ->build());

core::Property RetryFlowFile::MaximumRetries(core::PropertyBuilder::createProperty("Maximum Retries")
    ->withDescription("The maximum number of times a FlowFile can be retried before being passed to the 'retries_exceeded' relationship.")
    ->withDefaultValue<uint64_t>(3)
    ->supportsExpressionLanguage(true)
    ->build());

core::Property RetryFlowFile::PenalizeRetries(core::PropertyBuilder::createProperty("Penalize Retries")
  ->withDescription("If set to 'true', this Processor will penalize input FlowFiles before passing them to the 'retry' relationship. This does not apply to the 'retries_exceeded' relationship.")
  ->withDefaultValue<bool>(true)
  ->build());

core::Property RetryFlowFile::FailOnNonNumericalOverwrite(core::PropertyBuilder::createProperty("Fail on Non-numerical Overwrite")
    ->withDescription("If the FlowFile already has the attribute defined in 'Retry Attribute' that is *not* a number, fail the FlowFile instead of resetting that value to '1'")
    ->withDefaultValue<bool>(false)
    ->build());

core::Property RetryFlowFile::ReuseMode(core::PropertyBuilder::createProperty("Reuse Mode")
    ->withDescription(
        "Defines how the Processor behaves if the retry FlowFile has a different retry UUID than "
        "the instance that received the FlowFile. This generally means that the attribute was "
        "not reset after being successfully retried by a previous instance of this processor.")
    ->withAllowableValues<std::string>({FAIL_ON_REUSE, WARN_ON_REUSE, RESET_REUSE})
    ->withDefaultValue(FAIL_ON_REUSE)
    ->build());

core::Relationship RetryFlowFile::Retry("retry",
  "Input FlowFile has not exceeded the configured maximum retry count, pass this relationship back to the input Processor to create a limited feedback loop.");
core::Relationship RetryFlowFile::RetriesExceeded("retries_exceeded",
  "Input FlowFile has exceeded the configured maximum retry count, do not pass this relationship back to the input Processor to terminate the limited feedback loop.");
core::Relationship RetryFlowFile::Failure("failure",
    "The processor is configured such that a non-numerical value on 'Retry Attribute' results in a failure instead of resetting "
    "that value to '1'. This will immediately terminate the limited feedback loop. Might also include when 'Maximum Retries' contains "
    " attribute expression language that does not resolve to an Integer.");

void RetryFlowFile::initialize() {
  setSupportedProperties({
    RetryAttribute,
    MaximumRetries,
    PenalizeRetries,
    FailOnNonNumericalOverwrite,
    ReuseMode,
  });
  setSupportedRelationships({
    Retry,
    RetriesExceeded,
    Failure,
  });
}

void RetryFlowFile::onSchedule(core::ProcessContext* context, core::ProcessSessionFactory* /* sessionFactory */) {
  context->getProperty(RetryAttribute.getName(), retry_attribute_);
  context->getProperty(MaximumRetries.getName(), maximum_retries_);
  context->getProperty(PenalizeRetries.getName(), penalize_retries_);
  context->getProperty(FailOnNonNumericalOverwrite.getName(), fail_on_non_numerical_overwrite_);
  context->getProperty(ReuseMode.getName(), reuse_mode_);
  readDynamicPropertyKeys(context);
}

void RetryFlowFile::onTrigger(core::ProcessContext* context, core::ProcessSession* session) {
  std::shared_ptr<FlowFileRecord> flow_file = std::static_pointer_cast<FlowFileRecord> (session->get());
  if (!flow_file) {
    return;
  }

  bool failure_due_to_non_numerical_retry;
  utils::optional<uint64_t> maybe_retry_property_value = getRetryPropertyValue(flow_file);
  if (!maybe_retry_property_value) {
    session->transfer(flow_file, Failure);
    return;
  }
  uint64_t retry_property_value = maybe_retry_property_value.value_or(0);
  if (updateUUIDMarkerAndCheckFailOnReuse(flow_file, retry_property_value)) {
    session->transfer(flow_file, Failure);
    return;
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
    exceeded_flowfile_attribute_keys_.emplace_back(core::PropertyBuilder::createProperty(key)->withDescription("auto generated")->supportsExpressionLanguage(true)->build());
    logger_->log_info("RetryFlowFile registered attribute '%s'", key);
  }
}

utils::optional<uint64_t> RetryFlowFile::getRetryPropertyValue(const std::shared_ptr<FlowFileRecord>& flow_file) const {
  std::string value_as_string;
  try {
    if (flow_file->getAttribute(retry_attribute_, value_as_string)) {
      return utils::make_optional<uint64_t>(std::stoull(value_as_string));
    }
  }
  catch(const std::invalid_argument&) {
    if (fail_on_non_numerical_overwrite_) {
      logger_->log_info("Non-numerical retry property in RetryFlowFile. Sending flowfile to failure...", value_as_string);
      return {};
    }
    logger_->log_info("Non-numerical retry property in RetryFlowFile: overwriting %s with 0.", value_as_string);
  }
  catch(const std::out_of_range&) {
    logger_->log_error("Narrowing Exception for %s, treating it as non-numerical value", value_as_string);
  }
  return utils::make_optional<uint64_t>(0);
}

// Returns true on fail on reuse scenario
bool RetryFlowFile::updateUUIDMarkerAndCheckFailOnReuse(const std::shared_ptr<FlowFileRecord>& flow_file, uint64_t& retry_value) const {
  const std::string last_retried_by_property_name = retry_attribute_ + ".uuid";
  const std::string current_processor_uuid = getUUIDStr();
  std::string last_retried_by_uuid;
  if (flow_file->getAttribute(last_retried_by_property_name, last_retried_by_uuid)) {
    if (last_retried_by_uuid != current_processor_uuid) {
      if (reuse_mode_ == FAIL_ON_REUSE) {
        logger_->log_error("FlowFile %s was previously retried with the same attribute by a different "
            "processor (uuid: %s, current uuid: %s). Transfering flowfile to 'failure'...",
            flow_file->getUUIDStr(), last_retried_by_uuid, current_processor_uuid);
        return true;
      }
      if (reuse_mode_ == WARN_ON_REUSE) {
        logger_->log_warn("Reusing retry attribute that belongs to different processor. Resetting value to 0.");
      } else {  // Assuming reuse_mode_ == RESET_REUSE
        logger_->log_debug("Reusing retry attribute that belongs to different processor. Resetting value to 0.");
      }
      retry_value = 0;
    }
  }
  flow_file->setAttribute(last_retried_by_property_name, getUUIDStr());
  return false;
}

void RetryFlowFile::setRetriesExceededAttributesOnFlowFile(core::ProcessContext* context, const std::shared_ptr<FlowFileRecord>& flow_file) const {
  for (const auto& attribute : exceeded_flowfile_attribute_keys_) {
    std::string value;
    context->getDynamicProperty(attribute, value, flow_file);
    flow_file->setAttribute(attribute.getName(), value);
    logger_->log_info("Set attribute '%s' of flow file '%s' with value '%s'", attribute.getName(), flow_file->getUUIDStr(), value);
  }
}

} /* namespace processors */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
