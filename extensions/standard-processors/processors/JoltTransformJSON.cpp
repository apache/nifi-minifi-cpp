// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#include "JoltTransformJSON.h"


#include "core/Resource.h"
#include "rapidjson/document.h"
#include "rapidjson/error/en.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/writer.h"
#include "utils/ProcessorConfigUtils.h"
#include "utils/StringUtils.h"

namespace org::apache::nifi::minifi::processors {

void JoltTransformJSON::initialize() {
  setSupportedProperties(Properties);
  setSupportedRelationships(Relationships);
}

void JoltTransformJSON::onSchedule(core::ProcessContext& context, core::ProcessSessionFactory& /*session_factory*/) {
  transform_ = utils::parseEnumProperty<jolt_transform_json::JoltTransform>(context, JoltTransform);
  const std::string spec_str = utils::parseProperty(context, JoltSpecification);
  if (auto spec = utils::jolt::Spec::parse(spec_str, logger_)) {
    spec_ = std::move(spec.value());
  } else {
    throw Exception(PROCESS_SCHEDULE_EXCEPTION, fmt::format("The value of '{}' is not a valid jolt specification: {}", JoltSpecification.name, spec.error()));
  }
}

void JoltTransformJSON::onTrigger(core::ProcessContext& context, core::ProcessSession& session) {
  gsl_Expects(spec_);
  auto flowfile = session.get();
  if (!flowfile) {
    context.yield();
    return;
  }

  auto content = session.readBuffer(flowfile);
  rapidjson::Document input;
  rapidjson::ParseResult parse_result = input.Parse(reinterpret_cast<const char*>(content.buffer.data()), content.buffer.size());
  if (!parse_result) {
    logger_->log_warn("Failed to parse flowfile content as json: {} ({})", rapidjson::GetParseError_En(parse_result.Code()), gsl::narrow<size_t>(parse_result.Offset()));
    session.transfer(flowfile, Failure);
    return;
  }

  if (auto result = spec_->process(input, logger_)) {
    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    result.value().Accept(writer);
    session.writeBuffer(flowfile, std::span<const char>(buffer.GetString(), buffer.GetSize()));
    session.transfer(flowfile, Success);
  } else {
    logger_->log_info("Failed to apply transformation: {}", result.error());
    session.transfer(flowfile, Failure);
  }
}

REGISTER_RESOURCE(JoltTransformJSON, Processor);

}  // namespace org::apache::nifi::minifi::processors
