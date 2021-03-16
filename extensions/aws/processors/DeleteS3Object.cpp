/**
 * @file DeleteS3Object.cpp
 * DeleteS3Object class implementation
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

#include "DeleteS3Object.h"

#include <set>
#include <memory>

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace aws {
namespace processors {

const core::Property DeleteS3Object::ObjectKey(
  core::PropertyBuilder::createProperty("Object Key")
    ->withDescription("The key of the S3 object. If none is given the filename attribute will be used by default.")
    ->supportsExpressionLanguage(true)
    ->build());
const core::Property DeleteS3Object::Version(
  core::PropertyBuilder::createProperty("Version")
    ->withDescription("The Version of the Object to delete")
    ->supportsExpressionLanguage(true)
    ->build());

const core::Relationship DeleteS3Object::Success("success", "FlowFiles are routed to success relationship");
const core::Relationship DeleteS3Object::Failure("failure", "FlowFiles are routed to failure relationship");

void DeleteS3Object::initialize() {
  // Add new supported properties
  updateSupportedProperties({ObjectKey, Version});
  // Set the supported relationships
  setSupportedRelationships({Failure, Success});
}

void DeleteS3Object::onTrigger(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSession> &session) {
  logger_->log_debug("DeleteS3Object onTrigger");
  std::shared_ptr<core::FlowFile> flow_file = session->get();
  if (!flow_file) {
    context->yield();
    return;
  }

  auto common_properties = getCommonELSupportedProperties(context, flow_file);
  if (!common_properties) {
    session->transfer(flow_file, Failure);
    return;
  }

  std::string object_key;
  context->getProperty(ObjectKey, object_key, flow_file);
  if (object_key.empty() && (!flow_file->getAttribute("filename", object_key) || object_key.empty())) {
    logger_->log_error("No Object Key is set and default object key 'filename' attribute could not be found!");
    session->transfer(flow_file, Failure);
    return;
  }
  logger_->log_debug("DeleteS3Object: Object Key [%s]", object_key);

  std::string version;
  context->getProperty(Version, version, flow_file);
  logger_->log_debug("DeleteS3Object: Version [%s]", version);

  bool delete_succeeded = false;
  {
    std::lock_guard<std::mutex> lock(s3_wrapper_mutex_);
    configureS3Wrapper(common_properties.value());
    delete_succeeded = s3_wrapper_.deleteObject(common_properties->bucket, object_key, version);
  }

  if (delete_succeeded) {
    logger_->log_debug("Successfully deleted S3 object '%s' from bucket '%s'", object_key, common_properties->bucket);
    session->transfer(flow_file, Success);
  } else {
    logger_->log_error("Failed to delete S3 object '%s' from bucket '%s'", object_key, common_properties->bucket);
    session->transfer(flow_file, Failure);
  }
}

}  // namespace processors
}  // namespace aws
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
