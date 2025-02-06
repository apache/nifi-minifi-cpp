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

#include "utils/ProcessorConfigUtils.h"

#include "../GCPAttributes.h"
#include "core/FlowFile.h"
#include "core/ProcessContext.h"
#include "core/ProcessSession.h"
#include "core/Resource.h"

namespace gcs = ::google::cloud::storage;

namespace org::apache::nifi::minifi::extensions::gcp {
void DeleteGCSObject::initialize() {
  setSupportedProperties(Properties);
  setSupportedRelationships(Relationships);
}

void DeleteGCSObject::onTrigger(core::ProcessContext& context, core::ProcessSession& session) {
  gsl_Expects(gcp_credentials_);

  auto flow_file = session.get();
  if (!flow_file) {
    context.yield();
    return;
  }

  auto bucket = context.getProperty(Bucket, flow_file.get());
  if (!bucket || bucket->empty()) {
    logger_->log_error("Missing bucket name");
    session.transfer(flow_file, Failure);
    return;
  }
  auto object_name = context.getProperty(Key, flow_file.get());
  if (!object_name || object_name->empty()) {
    logger_->log_error("Missing object name");
    session.transfer(flow_file, Failure);
    return;
  }

  gcs::Generation generation;
  if (const auto object_generation_str =  context.getProperty(ObjectGeneration, flow_file.get()); object_generation_str && !object_generation_str->empty()) {
    if (const auto geni64 = parsing::parseIntegral<int64_t>(*object_generation_str)) {
      generation = gcs::Generation{*geni64};
    } else {
      logger_->log_error("Invalid generation: {}", *object_generation_str);
      session.transfer(flow_file, Failure);
      return;
    }
  }

  auto status = getClient().DeleteObject(*bucket, *object_name, generation, gcs::IfGenerationNotMatch(0));

  if (!status.ok()) {
    flow_file->setAttribute(GCS_STATUS_MESSAGE, status.message());
    flow_file->setAttribute(GCS_ERROR_REASON, status.error_info().reason());
    flow_file->setAttribute(GCS_ERROR_DOMAIN, status.error_info().domain());
    logger_->log_error("Failed to delete {} object from {} bucket on Google Cloud Storage {} {}", *object_name, *bucket, status.message(), status.error_info().reason());
    session.transfer(flow_file, Failure);
    return;
  }

  session.transfer(flow_file, Success);
}

REGISTER_RESOURCE(DeleteGCSObject, Processor);

}  // namespace org::apache::nifi::minifi::extensions::gcp
