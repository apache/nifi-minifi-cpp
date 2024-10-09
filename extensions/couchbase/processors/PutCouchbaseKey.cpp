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

#include "PutCouchbaseKey.h"
#include "utils/gsl.h"
#include "core/Resource.h"
#include "utils/ProcessorConfigUtils.h"

namespace org::apache::nifi::minifi::couchbase::processors {

void PutCouchbaseKey::onSchedule(core::ProcessContext& context, core::ProcessSessionFactory&) {
  couchbase_cluster_service_ = controllers::CouchbaseClusterService::getFromProperty(context, PutCouchbaseKey::CouchbaseClusterControllerService);
  persist_to_ = utils::parseEnumProperty<::couchbase::persist_to>(context, PutCouchbaseKey::PersistTo);
  replicate_to_ = utils::parseEnumProperty<::couchbase::replicate_to>(context, PutCouchbaseKey::ReplicateTo);
}

void PutCouchbaseKey::onTrigger(core::ProcessContext& context, core::ProcessSession& session) {
  gsl_Expects(couchbase_cluster_service_);

  auto flow_file = session.get();
  if (!flow_file) {
    context.yield();
    return;
  }

  CouchbaseCollection collection;
  if (!context.getProperty(BucketName, collection.bucket_name, flow_file.get()) || collection.bucket_name.empty()) {
    logger_->log_error("Bucket '{}' is invalid or empty!", collection.bucket_name);
    session.transfer(flow_file, Failure);
    return;
  }

  if (!context.getProperty(ScopeName, collection.scope_name, flow_file.get()) || collection.scope_name.empty()) {
    collection.scope_name = ::couchbase::scope::default_name;
  }

  if (!context.getProperty(CollectionName, collection.collection_name, flow_file.get()) || collection.collection_name.empty()) {
    collection.collection_name = ::couchbase::collection::default_name;
  }

  std::string document_id;
  if (!context.getProperty(DocumentId, document_id, flow_file.get()) || document_id.empty()) {
    document_id = flow_file->getUUIDStr();
  }

  ::couchbase::upsert_options options;
  options.durability(persist_to_, replicate_to_);
  auto result = session.readBuffer(flow_file);
  if (auto upsert_result = couchbase_cluster_service_->upsert(collection, document_id, result.buffer, options)) {
    session.putAttribute(*flow_file, "couchbase.bucket", upsert_result->bucket_name);
    session.putAttribute(*flow_file, "couchbase.doc.id", document_id);
    session.putAttribute(*flow_file, "couchbase.doc.cas", std::to_string(upsert_result->cas));
    session.putAttribute(*flow_file, "couchbase.doc.sequence.number", std::to_string(upsert_result->sequence_number));
    session.putAttribute(*flow_file, "couchbase.partition.uuid", std::to_string(upsert_result->partition_uuid));
    session.putAttribute(*flow_file, "couchbase.partition.id", std::to_string(upsert_result->partition_id));
    session.transfer(flow_file, Success);
  } else if (upsert_result.error() == CouchbaseErrorType::TEMPORARY) {
    logger_->log_error("Failed to upsert document '{}' to collection '{}.{}.{}' due to temporary issue, transferring to retry relationship",
      document_id, collection.bucket_name, collection.scope_name, collection.collection_name);
    session.transfer(flow_file, Retry);
  } else {
    logger_->log_error("Failed to upsert document '{}' to collection '{}.{}.{}', transferring to failure relationship",
      document_id, collection.bucket_name, collection.scope_name, collection.collection_name);
    session.transfer(flow_file, Failure);
  }
}

REGISTER_RESOURCE(PutCouchbaseKey, Processor);

}  // namespace org::apache::nifi::minifi::couchbase::processors
