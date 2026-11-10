// SPDX-License-Identifier: Apache-2.0
// SPDX-FileCopyrightText: See the NOTICE file distributed with this work for additional information regarding copyright ownership
// SPDX-FileContributor: Licensed to the Apache Software Foundation (ASF) under one or more contributor license agreements

#include "DeleteGCSObject.h"

#include "../GCPAttributes.h"
#include "api/core/ProcessContext.h"
#include "api/core/ProcessSession.h"
#include "api/core/Resource.h"
#include "api/utils/ProcessorConfigUtils.h"

namespace gcs = ::google::cloud::storage;

namespace org::apache::nifi::minifi::extensions::gcp {

minifi_status DeleteGCSObject::onTriggerImpl(api::core::ProcessContext& context, api::core::ProcessSession& session) {
  gsl_Expects(gcp_credentials_);

  auto flow_file = session.get();
  if (!flow_file) {
    return MINIFI_STATUS_PROCESSOR_YIELD;
  }

  auto bucket = api::utils::parseOptionalProperty(context, Bucket, &flow_file);
  if (!bucket || bucket->empty()) {
    logger_->log_error("Missing bucket name");
    session.transfer(std::move(flow_file), Failure);
    return MINIFI_STATUS_SUCCESS;
  }
  auto object_name = api::utils::parseOptionalProperty(context, Key, &flow_file);
  if (!object_name || object_name->empty()) {
    logger_->log_error("Missing object name");
    session.transfer(std::move(flow_file), Failure);
    return MINIFI_STATUS_SUCCESS;
  }

  gcs::Generation generation;
  if (auto object_generation_str = api::utils::parseOptionalProperty(context, ObjectGeneration, &flow_file); object_generation_str && !object_generation_str->empty()) {
    if (const auto geni64 = parsing::parseIntegral<int64_t>(*object_generation_str)) {
      generation = gcs::Generation{*geni64};
    } else {
      logger_->log_error("Invalid generation: {}", *object_generation_str);
      session.transfer(std::move(flow_file), Failure);
      return MINIFI_STATUS_SUCCESS;
    }
  }

  auto status = getClient().DeleteObject(*bucket, *object_name, generation, gcs::IfGenerationNotMatch(0));

  if (!status.ok()) {
    session.setAttribute(flow_file, GCS_STATUS_MESSAGE, status.message());
    session.setAttribute(flow_file, GCS_ERROR_REASON, status.error_info().reason());
    session.setAttribute(flow_file, GCS_ERROR_DOMAIN, status.error_info().domain());
    logger_->log_error("Failed to delete {} object from {} bucket on Google Cloud Storage {} {}", *object_name, *bucket, status.message(), status.error_info().reason());
    session.transfer(std::move(flow_file), Failure);
    return MINIFI_STATUS_SUCCESS;
  }

  session.transfer(std::move(flow_file), Success);
  return MINIFI_STATUS_SUCCESS;
}

}  // namespace org::apache::nifi::minifi::extensions::gcp
