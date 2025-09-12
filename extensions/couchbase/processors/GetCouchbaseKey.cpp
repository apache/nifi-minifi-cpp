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

#include "GetCouchbaseKey.h"
#include "CouchbaseClusterService.h"
#include "minifi-cpp/utils/gsl.h"
#include "core/Resource.h"
#include "utils/ProcessorConfigUtils.h"

namespace org::apache::nifi::minifi::couchbase::processors {

void GetCouchbaseKey::onSchedule(core::ProcessContext& context, core::ProcessSessionFactory&) {
  couchbase_cluster_service_ = utils::parseControllerService<controllers::CouchbaseClusterService>(context, GetCouchbaseKey::CouchbaseClusterControllerService, context.getProcessorInfo().getUUID());
  document_type_ = utils::parseEnumProperty<CouchbaseValueType>(context, GetCouchbaseKey::DocumentType);
}

void GetCouchbaseKey::onTrigger(core::ProcessContext& context, core::ProcessSession& session) {
  gsl_Expects(couchbase_cluster_service_);

  auto flow_file = session.get();
  if (!flow_file) {
    context.yield();
    return;
  }

  CouchbaseCollection collection;
  if (const auto bucket_name = utils::parseOptionalProperty(context, BucketName, flow_file.get())) {
    collection.bucket_name = *bucket_name;
  } else {
    logger_->log_error("Bucket '{}' is invalid or empty!", collection.bucket_name);
    session.transfer(flow_file, Failure);
    return;
  }

  collection.scope_name = utils::parseOptionalProperty(context, ScopeName, flow_file.get()).value_or(::couchbase::scope::default_name);
  collection.collection_name = utils::parseOptionalProperty(context, CollectionName, flow_file.get()).value_or(::couchbase::collection::default_name);
  std::string document_id = utils::parseOptionalProperty(context, DocumentId, flow_file.get()) | utils::valueOrElse([&flow_file, &session] {
    const auto ff_content = session.readBuffer(flow_file).buffer;
    return std::string(reinterpret_cast<const char*>(ff_content.data()), ff_content.size());
  });

  if (document_id.empty()) {
    logger_->log_error("Document ID is empty, transferring FlowFile to failure relationship");
    session.transfer(flow_file, Failure);
    return;
  }

  std::string attribute_to_put_result_to = utils::parseOptionalProperty(context, PutValueToAttribute, flow_file.get()).value_or("");

  if (auto get_result = couchbase_cluster_service_->get(collection, document_id, document_type_)) {
    if (!attribute_to_put_result_to.empty()) {
      if (document_type_ == CouchbaseValueType::String) {
        session.putAttribute(*flow_file, attribute_to_put_result_to, std::get<std::string>(get_result->value));
      } else {
        auto& binary_data = std::get<std::vector<std::byte>>(get_result->value);
        std::string str_value{reinterpret_cast<const char*>(binary_data.data()), binary_data.size()};
        session.putAttribute(*flow_file, attribute_to_put_result_to, str_value);
      }
    } else {
      session.write(flow_file, [&, this](const std::shared_ptr<io::OutputStream>& stream) -> int64_t {
        if (document_type_ == CouchbaseValueType::String) {
          auto& value = std::get<std::string>(get_result->value);
          stream->write(value);
          return gsl::narrow<int64_t>(value.size());
        } else {
          auto& value = std::get<std::vector<std::byte>>(get_result->value);
          stream->write(value);
          return gsl::narrow<int64_t>(value.size());
        }
      });
    }

    session.putAttribute(*flow_file, "couchbase.bucket", get_result->bucket_name);
    session.putAttribute(*flow_file, "couchbase.doc.id", document_id);
    session.putAttribute(*flow_file, "couchbase.doc.cas", std::to_string(get_result->cas));
    session.putAttribute(*flow_file, "couchbase.doc.expiry", get_result->expiry);
    session.transfer(flow_file, Success);
  } else if (get_result.error() == CouchbaseErrorType::TEMPORARY) {
    logger_->log_error("Failed to get document '{}' from collection '{}.{}.{}' due to timeout, transferring to retry relationship",
      document_id, collection.bucket_name, collection.scope_name, collection.collection_name);
    session.transfer(flow_file, Retry);
  } else {
    logger_->log_error("Failed to get document '{}' from collection '{}.{}.{}', transferring to failure relationship",
      document_id, collection.bucket_name, collection.scope_name, collection.collection_name);
    session.transfer(flow_file, Failure);
  }
}

REGISTER_RESOURCE(GetCouchbaseKey, Processor);

}  // namespace org::apache::nifi::minifi::couchbase::processors
