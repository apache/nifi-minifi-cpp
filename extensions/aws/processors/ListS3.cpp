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

#include "ListS3.h"

#include <algorithm>
#include <utility>
#include <memory>

#include "core/ProcessContext.h"
#include "core/ProcessSession.h"
#include "core/Resource.h"

namespace org::apache::nifi::minifi::aws::processors {

void ListS3::initialize() {
  setSupportedProperties(Properties);
  setSupportedRelationships(Relationships);
}

void ListS3::onSchedule(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSessionFactory> &sessionFactory) {
  S3Processor::onSchedule(context, sessionFactory);

  auto state_manager = context->getStateManager();
  if (state_manager == nullptr) {
    throw Exception(PROCESSOR_EXCEPTION, "Failed to get StateManager");
  }
  state_manager_ = std::make_unique<minifi::utils::ListingStateManager>(state_manager);

  auto common_properties = getCommonELSupportedProperties(context, nullptr);
  if (!common_properties) {
    throw Exception(PROCESS_SCHEDULE_EXCEPTION, "Required property is not set or invalid");
  }

  gsl_Expects(client_config_);
  list_request_params_ = std::make_unique<aws::s3::ListRequestParameters>(common_properties->credentials, *client_config_);
  list_request_params_->setClientConfig(common_properties->proxy, common_properties->endpoint_override_url);
  list_request_params_->bucket = common_properties->bucket;

  context->getProperty(Delimiter, list_request_params_->delimiter);
  logger_->log_debug("ListS3: Delimiter [%s]", list_request_params_->delimiter);

  context->getProperty(Prefix, list_request_params_->prefix);
  logger_->log_debug("ListS3: Prefix [%s]", list_request_params_->prefix);

  context->getProperty(UseVersions, list_request_params_->use_versions);
  logger_->log_debug("ListS3: UseVersions [%s]", list_request_params_->use_versions ? "true" : "false");

  if (auto minimum_object_age = context->getProperty<core::TimePeriodValue>(MinimumObjectAge)) {
    list_request_params_->min_object_age = minimum_object_age->getMilliseconds().count();
  } else {
    throw Exception(PROCESS_SCHEDULE_EXCEPTION, "Minimum Object Age missing or invalid");
  }
  logger_->log_debug("S3Processor: Minimum Object Age [%llud]", list_request_params_->min_object_age);

  context->getProperty(WriteObjectTags, write_object_tags_);
  logger_->log_debug("ListS3: WriteObjectTags [%s]", write_object_tags_ ? "true" : "false");

  context->getProperty(WriteUserMetadata, write_user_metadata_);
  logger_->log_debug("ListS3: WriteUserMetadata [%s]", write_user_metadata_ ? "true" : "false");

  context->getProperty(RequesterPays, requester_pays_);
  logger_->log_debug("ListS3: RequesterPays [%s]", requester_pays_ ? "true" : "false");
}

void ListS3::writeObjectTags(
    const aws::s3::ListedObjectAttributes &object_attributes,
    core::ProcessSession &session,
    const std::shared_ptr<core::FlowFile> &flow_file) {
  if (!write_object_tags_) {
    return;
  }

  aws::s3::GetObjectTagsParameters params(list_request_params_->credentials, list_request_params_->client_config);
  params.bucket = list_request_params_->bucket;
  params.object_key = object_attributes.filename;
  params.version = object_attributes.version;
  auto get_object_tags_result = s3_wrapper_.getObjectTags(params);
  if (get_object_tags_result) {
    for (const auto& tag : *get_object_tags_result) {
      session.putAttribute(flow_file, "s3.tag." + tag.first, tag.second);
    }
  } else {
    logger_->log_warn("Failed to get object tags for object %s in bucket %s", object_attributes.filename, params.bucket);
  }
}

void ListS3::writeUserMetadata(
    const aws::s3::ListedObjectAttributes &object_attributes,
    core::ProcessSession &session,
    const std::shared_ptr<core::FlowFile> &flow_file) {
  if (!write_user_metadata_) {
    return;
  }

  aws::s3::HeadObjectRequestParameters params(list_request_params_->credentials, list_request_params_->client_config);
  params.bucket = list_request_params_->bucket;
  params.object_key = object_attributes.filename;
  params.version = object_attributes.version;
  params.requester_pays = requester_pays_;
  auto head_object_tags_result = s3_wrapper_.headObject(params);
  if (head_object_tags_result) {
    for (const auto& metadata : head_object_tags_result->user_metadata_map) {
      session.putAttribute(flow_file, "s3.user.metadata." + metadata.first, metadata.second);
    }
  } else {
    logger_->log_warn("Failed to get object metadata for object %s in bucket %s", params.object_key, params.bucket);
  }
}

void ListS3::createNewFlowFile(
    core::ProcessSession &session,
    const aws::s3::ListedObjectAttributes &object_attributes) {
  auto flow_file = session.create();
  session.putAttribute(flow_file, "s3.bucket", list_request_params_->bucket);
  session.putAttribute(flow_file, core::SpecialFlowAttribute::FILENAME, object_attributes.filename);
  session.putAttribute(flow_file, "s3.etag", object_attributes.etag);
  session.putAttribute(flow_file, "s3.isLatest", object_attributes.is_latest ? "true" : "false");
  session.putAttribute(flow_file, "s3.lastModified", std::to_string(object_attributes.last_modified.time_since_epoch() / std::chrono::milliseconds(1)));
  session.putAttribute(flow_file, "s3.length", std::to_string(object_attributes.length));
  session.putAttribute(flow_file, "s3.storeClass", object_attributes.store_class);
  if (!object_attributes.version.empty()) {
    session.putAttribute(flow_file, "s3.version", object_attributes.version);
  }
  writeObjectTags(object_attributes, session, flow_file);
  writeUserMetadata(object_attributes, session, flow_file);

  session.transfer(flow_file, Success);
}

void ListS3::onTrigger(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSession> &session) {
  logger_->log_trace("ListS3 onTrigger");

  auto aws_results = s3_wrapper_.listBucket(*list_request_params_);
  if (!aws_results) {
    logger_->log_error("Failed to list S3 bucket %s", list_request_params_->bucket);
    context->yield();
    return;
  }

  auto stored_listing_state = state_manager_->getCurrentState();
  auto latest_listing_state = stored_listing_state;
  std::size_t files_transferred = 0;

  for (const auto& object_attributes : *aws_results) {
    if (stored_listing_state.wasObjectListedAlready(object_attributes)) {
      continue;
    }

    createNewFlowFile(*session, object_attributes);
    ++files_transferred;
    latest_listing_state.updateState(object_attributes);
  }

  logger_->log_debug("ListS3 transferred %zu flow files", files_transferred);
  state_manager_->storeState(latest_listing_state);

  if (files_transferred == 0) {
    logger_->log_debug("No new S3 objects were found in bucket %s to list", list_request_params_->bucket);
    context->yield();
    return;
  }
}

REGISTER_RESOURCE(ListS3, Processor);

}  // namespace org::apache::nifi::minifi::aws::processors
