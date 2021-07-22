/**
 * @file ListS3.cpp
 * ListS3 class implementation
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

#include "ListS3.h"

#include <tuple>
#include <algorithm>
#include <set>
#include <utility>
#include <memory>

#include "utils/StringUtils.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace aws {
namespace processors {

const std::string ListS3::LATEST_LISTED_KEY_PREFIX = "listed_key.";
const std::string ListS3::LATEST_LISTED_KEY_TIMESTAMP = "listed_timestamp";

const core::Property ListS3::Delimiter(
  core::PropertyBuilder::createProperty("Delimiter")
    ->withDescription("The string used to delimit directories within the bucket. Please consult the AWS documentation for the correct use of this field.")
    ->build());
const core::Property ListS3::Prefix(
  core::PropertyBuilder::createProperty("Prefix")
    ->withDescription("The prefix used to filter the object list. In most cases, it should end with a forward slash ('/').")
    ->build());
const core::Property ListS3::UseVersions(
  core::PropertyBuilder::createProperty("Use Versions")
    ->isRequired(true)
    ->withDefaultValue<bool>(false)
    ->withDescription("Specifies whether to use S3 versions, if applicable. If false, only the latest version of each object will be returned.")
    ->build());
const core::Property ListS3::MinimumObjectAge(
  core::PropertyBuilder::createProperty("Minimum Object Age")
    ->isRequired(true)
    ->withDefaultValue<core::TimePeriodValue>("0 sec")
    ->withDescription("The minimum age that an S3 object must be in order to be considered; any object younger than this amount of time (according to last modification date) will be ignored.")
    ->build());
const core::Property ListS3::WriteObjectTags(
  core::PropertyBuilder::createProperty("Write Object Tags")
    ->isRequired(true)
    ->withDefaultValue<bool>(false)
    ->withDescription("If set to 'true', the tags associated with the S3 object will be written as FlowFile attributes.")
    ->build());
const core::Property ListS3::WriteUserMetadata(
  core::PropertyBuilder::createProperty("Write User Metadata")
    ->isRequired(true)
    ->withDefaultValue<bool>(false)
    ->withDescription("If set to 'true', the user defined metadata associated with the S3 object will be added to FlowFile attributes/records.")
    ->build());
const core::Property ListS3::RequesterPays(
  core::PropertyBuilder::createProperty("Requester Pays")
    ->isRequired(true)
    ->withDefaultValue<bool>(false)
    ->withDescription("If true, indicates that the requester consents to pay any charges associated with listing the S3 bucket. This sets the 'x-amz-request-payer' header to 'requester'. "
                      "Note that this setting is only used if Write User Metadata is true.")
    ->build());

const core::Relationship ListS3::Success("success", "FlowFiles are routed to success relationship");

void ListS3::initialize() {
  // Add new supported properties
  updateSupportedProperties({Delimiter, Prefix, UseVersions, MinimumObjectAge, WriteObjectTags, WriteUserMetadata, RequesterPays});
  // Set the supported relationships
  setSupportedRelationships({Success});
}

void ListS3::onSchedule(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSessionFactory> &sessionFactory) {
  S3Processor::onSchedule(context, sessionFactory);

  state_manager_ = context->getStateManager();
  if (state_manager_ == nullptr) {
    throw Exception(PROCESSOR_EXCEPTION, "Failed to get StateManager");
  }

  auto common_properties = getCommonELSupportedProperties(context, nullptr);
  if (!common_properties) {
    throw Exception(PROCESS_SCHEDULE_EXCEPTION, "Required property is not set or invalid");
  }
  list_request_params_ = std::make_unique<aws::s3::ListRequestParameters>(common_properties->credentials, client_config_);
  list_request_params_->setClientConfig(common_properties->proxy, common_properties->endpoint_override_url);
  list_request_params_->bucket = common_properties->bucket;

  context->getProperty(Delimiter.getName(), list_request_params_->delimiter);
  logger_->log_debug("ListS3: Delimiter [%s]", list_request_params_->delimiter);

  context->getProperty(Prefix.getName(), list_request_params_->prefix);
  logger_->log_debug("ListS3: Prefix [%s]", list_request_params_->prefix);

  context->getProperty(UseVersions.getName(), list_request_params_->use_versions);
  logger_->log_debug("ListS3: UseVersions [%s]", list_request_params_->use_versions ? "true" : "false");

  std::string min_obj_age_str;
  if (!context->getProperty(MinimumObjectAge.getName(), min_obj_age_str) || min_obj_age_str.empty() || !core::Property::getTimeMSFromString(min_obj_age_str, list_request_params_->min_object_age)) {
    throw Exception(PROCESS_SCHEDULE_EXCEPTION, "Minimum Object Age missing or invalid");
  }
  logger_->log_debug("S3Processor: Minimum Object Age [%llud]", min_obj_age_str, list_request_params_->min_object_age);

  context->getProperty(WriteObjectTags.getName(), write_object_tags_);
  logger_->log_debug("ListS3: WriteObjectTags [%s]", write_object_tags_ ? "true" : "false");

  context->getProperty(WriteUserMetadata.getName(), write_user_metadata_);
  logger_->log_debug("ListS3: WriteUserMetadata [%s]", write_user_metadata_ ? "true" : "false");

  context->getProperty(RequesterPays.getName(), requester_pays_);
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

std::vector<std::string> ListS3::getLatestListedKeys(const std::unordered_map<std::string, std::string> &state) {
  std::vector<std::string> latest_listed_keys;
  for (const auto& kvp : state) {
    if (kvp.first.rfind(LATEST_LISTED_KEY_PREFIX, 0) == 0) {
      latest_listed_keys.push_back(kvp.second);
    }
  }
  return latest_listed_keys;
}

uint64_t ListS3::getLatestListedKeyTimestamp(const std::unordered_map<std::string, std::string> &state) {
  std::string stored_listed_key_timestamp_str;
  auto it = state.find(LATEST_LISTED_KEY_TIMESTAMP);
  if (it != state.end()) {
    stored_listed_key_timestamp_str = it->second;
  }

  int64_t stored_listed_key_timestamp = 0;
  core::Property::StringToInt(stored_listed_key_timestamp_str, stored_listed_key_timestamp);

  return stored_listed_key_timestamp;
}

ListS3::ListingState ListS3::getCurrentState(const std::shared_ptr<core::ProcessContext>& /*context*/) {
  ListS3::ListingState current_listing_state;
  std::unordered_map<std::string, std::string> state;
  if (!state_manager_->get(state)) {
    logger_->log_info("No stored state for listed objects was found");
    return current_listing_state;
  }

  current_listing_state.listed_key_timestamp = getLatestListedKeyTimestamp(state);
  logger_->log_debug("Restored previous listed timestamp %lld", current_listing_state.listed_key_timestamp);

  current_listing_state.listed_keys = getLatestListedKeys(state);
  return current_listing_state;
}

void ListS3::storeState(const ListS3::ListingState &latest_listing_state) {
  std::unordered_map<std::string, std::string> state;
  state[LATEST_LISTED_KEY_TIMESTAMP] = std::to_string(latest_listing_state.listed_key_timestamp);
  for (std::size_t i = 0; i < latest_listing_state.listed_keys.size(); ++i) {
    state[LATEST_LISTED_KEY_PREFIX + std::to_string(i)] = latest_listing_state.listed_keys.at(i);
  }
  logger_->log_debug("Stored new listed timestamp %lld", latest_listing_state.listed_key_timestamp);
  state_manager_->set(state);
}

void ListS3::createNewFlowFile(
    core::ProcessSession &session,
    const aws::s3::ListedObjectAttributes &object_attributes) {
  auto flow_file = session.create();
  session.putAttribute(flow_file, "s3.bucket", list_request_params_->bucket);
  session.putAttribute(flow_file, core::SpecialFlowAttribute::FILENAME, object_attributes.filename);
  session.putAttribute(flow_file, "s3.etag", object_attributes.etag);
  session.putAttribute(flow_file, "s3.isLatest", object_attributes.is_latest ? "true" : "false");
  session.putAttribute(flow_file, "s3.lastModified", std::to_string(object_attributes.last_modified));
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
  logger_->log_debug("ListS3 onTrigger");

  auto aws_results = s3_wrapper_.listBucket(*list_request_params_);
  if (!aws_results) {
    logger_->log_error("Failed to list S3 bucket %s", list_request_params_->bucket);
    context->yield();
    return;
  }

  auto stored_listing_state = getCurrentState(context);
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
  storeState(latest_listing_state);

  if (files_transferred == 0) {
    logger_->log_debug("No new S3 objects were found in bucket %s to list", list_request_params_->bucket);
    context->yield();
    return;
  }
}

bool ListS3::ListingState::wasObjectListedAlready(const aws::s3::ListedObjectAttributes &object_attributes) const {
  return listed_key_timestamp > object_attributes.last_modified ||
      (listed_key_timestamp == object_attributes.last_modified &&
        std::find(listed_keys.begin(), listed_keys.end(), object_attributes.filename) != listed_keys.end());
}

void ListS3::ListingState::updateState(const aws::s3::ListedObjectAttributes &object_attributes) {
  if (listed_key_timestamp < object_attributes.last_modified) {
    listed_key_timestamp = object_attributes.last_modified;
    listed_keys.clear();
    listed_keys.push_back(object_attributes.filename);
  } else if (listed_key_timestamp == object_attributes.last_modified) {
    listed_keys.push_back(object_attributes.filename);
  }
}

REGISTER_RESOURCE(ListS3, "This Processor retrieves a listing of objects from an Amazon S3 bucket.");

}  // namespace processors
}  // namespace aws
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
