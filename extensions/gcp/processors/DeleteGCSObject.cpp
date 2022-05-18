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

#include "DeleteGCSObject.h"

#include "core/Resource.h"
#include "core/ProcessContext.h"
#include "core/ProcessSession.h"
#include "core/FlowFile.h"
#include "../GCPAttributes.h"

namespace gcs = ::google::cloud::storage;

namespace org::apache::nifi::minifi::extensions::gcp {
const core::Property DeleteGCSObject::Bucket(
    core::PropertyBuilder::createProperty("Bucket")
        ->withDescription("Bucket of the object.")
        ->withDefaultValue("${gcs.bucket}")
        ->supportsExpressionLanguage(true)
        ->build());

const core::Property DeleteGCSObject::Key(
    core::PropertyBuilder::createProperty("Key")
        ->withDescription("Name of the object.")
        ->withDefaultValue("${filename}")
        ->supportsExpressionLanguage(true)
        ->build());

const core::Property DeleteGCSObject::ObjectGeneration(
    core::PropertyBuilder::createProperty("Object Generation")
        ->withDescription("The generation of the Object to download. If left empty, then it will download the latest generation.")
        ->supportsExpressionLanguage(true)
        ->build());

const core::Property DeleteGCSObject::EncryptionKey(
    core::PropertyBuilder::createProperty("Server Side Encryption Key")
        ->withDescription("The AES256 Encryption Key (encoded in base64) for server-side decryption of the object.")
        ->isRequired(false)
        ->supportsExpressionLanguage(true)
        ->build());

const core::Relationship DeleteGCSObject::Success("success", "FlowFiles are routed to this relationship after a successful Google Cloud Storage operation.");
const core::Relationship DeleteGCSObject::Failure("failure", "FlowFiles are routed to this relationship if the Google Cloud Storage operation fails.");

void DeleteGCSObject::initialize() {
  setSupportedProperties({GCPCredentials,
                          Bucket,
                          Key,
                          ObjectGeneration,
                          NumberOfRetries,
                          EncryptionKey,
                          EndpointOverrideURL});
  setSupportedRelationships({Success, Failure});
}

void DeleteGCSObject::onTrigger(const std::shared_ptr<core::ProcessContext>& context, const std::shared_ptr<core::ProcessSession>& session) {
  gsl_Expects(context && session && gcp_credentials_);

  auto flow_file = session->get();
  if (!flow_file) {
    context->yield();
    return;
  }

  auto bucket = context->getProperty(Bucket, flow_file);
  if (!bucket || bucket->empty()) {
    logger_->log_error("Missing bucket name");
    session->transfer(flow_file, Failure);
    return;
  }
  auto object_name = context->getProperty(Key, flow_file);
  if (!object_name || object_name->empty()) {
    logger_->log_error("Missing object name");
    session->transfer(flow_file, Failure);
    return;
  }

  gcs::Generation generation;

  if (auto gen_str = context->getProperty(ObjectGeneration, flow_file); gen_str && !gen_str->empty()) {
    try {
      uint64_t gen;
      utils::internal::ValueParser(*gen_str).parse(gen).parseEnd();
      generation = gcs::Generation(gen);
    } catch (const utils::internal::ValueException&) {
      logger_->log_error("Invalid generation: %s", *gen_str);
      session->transfer(flow_file, Failure);
      return;
    }
  }

  auto status = getClient().DeleteObject(*bucket, *object_name, generation, gcs::IfGenerationNotMatch(0));

  if (!status.ok()) {
    flow_file->setAttribute(GCS_STATUS_MESSAGE, status.message());
    flow_file->setAttribute(GCS_ERROR_REASON, status.error_info().reason());
    flow_file->setAttribute(GCS_ERROR_DOMAIN, status.error_info().domain());
    logger_->log_error("Failed to delete %s object from %s bucket on Google Cloud Storage %s %s", *object_name, *bucket, status.message(), status.error_info().reason());
    session->transfer(flow_file, Failure);
    return;
  }

  session->transfer(flow_file, Success);
}

REGISTER_RESOURCE(DeleteGCSObject, "Deletes an object from a Google Cloud Bucket.");
}  // namespace org::apache::nifi::minifi::extensions::gcp
