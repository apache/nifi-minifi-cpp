/**
 * @file GenerateFlowFile.h
 * GenerateFlowFile class declaration
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

#include <string>

#include "KamikazeProcessor.h"
#include "Exception.h"
#include "core/Resource.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace processors {

const std::string KamikazeProcessor::OnScheduleExceptionStr = "This processor was configured to throw exception during onSchedule";
const std::string KamikazeProcessor::OnTriggerExceptionStr = "This processor was configured to throw exception during onTrigger";
const std::string KamikazeProcessor::OnScheduleLogStr = "KamikazeProcessor::onSchedule executed";
const std::string KamikazeProcessor::OnTriggerLogStr = "KamikazeProcessor::onTrigger executed";
const std::string KamikazeProcessor::OnUnScheduleLogStr = "KamikazeProcessor::onUnSchedule";

core::Property KamikazeProcessor::ThrowInOnSchedule(
    core::PropertyBuilder::createProperty("Throw in onSchedule")
    ->withDescription("Set to throw expcetion during onSchedule call")
    ->isRequired(true)
    ->withDefaultValue<bool>(false)->build());

core::Property KamikazeProcessor::ThrowInOnTrigger(
    core::PropertyBuilder::createProperty("Throw in onTrigger")
    ->withDescription("Set to throw expcetion during onTrigger call")
    ->isRequired(true)
    ->withDefaultValue<bool>(false)->build());

void KamikazeProcessor::initialize() {
  setSupportedProperties({ThrowInOnSchedule, ThrowInOnTrigger});
}

void KamikazeProcessor::onSchedule(core::ProcessContext *context, core::ProcessSessionFactory* /*sessionFactory*/) {
  std::string value;
  context->getProperty(ThrowInOnTrigger.getName(), value);
  _throwInOnTrigger = utils::StringUtils::toBool(value).value_or(false);

  context->getProperty(ThrowInOnSchedule.getName(), value);

  if (utils::StringUtils::toBool(value).value_or(false)) {
    throw Exception(PROCESS_SCHEDULE_EXCEPTION, OnScheduleExceptionStr);
  }
  logger_->log_error("%s", OnScheduleLogStr);
}

void KamikazeProcessor::onTrigger(core::ProcessContext *, core::ProcessSession *) {
  if (_throwInOnTrigger) {
    throw Exception(PROCESSOR_EXCEPTION, OnTriggerExceptionStr);
  }
  logger_->log_error("%s", OnTriggerLogStr);
}

void KamikazeProcessor::onUnSchedule() {
  logger_->log_error("%s", OnUnScheduleLogStr);
}

REGISTER_RESOURCE(KamikazeProcessor, "This processor can throw exceptions in onTrigger and onSchedule calls based on configration. Only for testing purposes.");

}  // namespace processors
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
