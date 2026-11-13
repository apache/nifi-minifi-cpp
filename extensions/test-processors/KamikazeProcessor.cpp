// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#include "KamikazeProcessor.h"


#include <string>

#include "minifi-cpp/Exception.h"
#include "minifi-cpp/core/ProcessContext.h"
#include "core/Resource.h"
#include "utils/ProcessorConfigUtils.h"

namespace org::apache::nifi::minifi::processors {

const std::string KamikazeProcessor::OnScheduleExceptionStr = "This processor was configured to throw exception during onSchedule";
const std::string KamikazeProcessor::OnTriggerExceptionStr = "This processor was configured to throw exception during onTrigger";
const std::string KamikazeProcessor::OnScheduleLogStr = "KamikazeProcessor::onSchedule executed";
const std::string KamikazeProcessor::OnTriggerLogStr = "KamikazeProcessor::onTrigger executed";
const std::string KamikazeProcessor::OnUnScheduleLogStr = "KamikazeProcessor::onUnSchedule";

void KamikazeProcessor::initialize() {
  setSupportedProperties(Properties);
}

void KamikazeProcessor::onSchedule(core::ProcessContext& context, core::ProcessSessionFactory&) {
  _throwInOnTrigger = utils::parseBoolProperty(context, ThrowInOnTrigger);

  if (utils::parseBoolProperty(context, ThrowInOnSchedule)) {
    throw Exception(PROCESS_SCHEDULE_EXCEPTION, OnScheduleExceptionStr);
  }

  logger_->log_error("{}", OnScheduleLogStr);
}

void KamikazeProcessor::onTrigger(core::ProcessContext&, core::ProcessSession&) {
  if (_throwInOnTrigger) {
    throw Exception(PROCESSOR_EXCEPTION, OnTriggerExceptionStr);
  }
  logger_->log_error("{}", OnTriggerLogStr);
}

void KamikazeProcessor::onUnSchedule() {
  logger_->log_error("{}", OnUnScheduleLogStr);
}

REGISTER_RESOURCE(KamikazeProcessor, Processor);

}  // namespace org::apache::nifi::minifi::processors
