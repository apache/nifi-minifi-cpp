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
#pragma once

#include <memory>
#include <string>
#include <utility>

#include "core/AbstractProcessor.h"
#include "core/ProcessSession.h"
#include "utils/Enum.h"
#include "core/logging/LoggerFactory.h"
#include "CouchbaseClusterService.h"
#include "couchbase/persist_to.hxx"
#include "couchbase/replicate_to.hxx"

namespace magic_enum::customize {

template <>
constexpr customize_t enum_name<::couchbase::persist_to>(::couchbase::persist_to value) noexcept {
  switch (value) {
    case ::couchbase::persist_to::none:
      return "NONE";
    case ::couchbase::persist_to::active:
      return "ACTIVE";
    case ::couchbase::persist_to::one:
      return "ONE";
    case ::couchbase::persist_to::two:
      return "TWO";
    case ::couchbase::persist_to::three:
      return "THREE";
    case ::couchbase::persist_to::four:
      return "FOUR";
  }
  return invalid_tag;
}

template <>
constexpr customize_t enum_name<::couchbase::replicate_to>(::couchbase::replicate_to value) noexcept {
  switch (value) {
    case ::couchbase::replicate_to::none:
      return "NONE";
    case ::couchbase::replicate_to::one:
      return "ONE";
    case ::couchbase::replicate_to::two:
      return "TWO";
    case ::couchbase::replicate_to::three:
      return "THREE";
  }
  return invalid_tag;
}
}  // namespace magic_enum::customize

namespace org::apache::nifi::minifi::couchbase::processors {

class PutCouchbaseKey final : public core::AbstractProcessor<PutCouchbaseKey> {
 public:
  using core::AbstractProcessor<PutCouchbaseKey>::AbstractProcessor;

  EXTENSIONAPI static constexpr const char* Description = "Put a document to Couchbase Server via Key/Value access.";

  EXTENSIONAPI static constexpr auto CouchbaseClusterControllerService = core::PropertyDefinitionBuilder<>::createProperty("Couchbase Cluster Controller Service")
      .withDescription("A Couchbase Cluster Controller Service which manages connections to a Couchbase cluster.")
      .withAllowedTypes<controllers::CouchbaseClusterService>()
      .isRequired(true)
      .build();
  EXTENSIONAPI static constexpr auto BucketName = core::PropertyDefinitionBuilder<>::createProperty("Bucket Name")
      .withDescription("The name of bucket to access.")
      .withDefaultValue("default")
      .withValidator(core::StandardPropertyTypes::NON_BLANK_VALIDATOR)
      .isRequired(true)
      .supportsExpressionLanguage(true)
      .build();
  EXTENSIONAPI static constexpr auto ScopeName = core::PropertyDefinitionBuilder<>::createProperty("Scope Name")
      .withDescription("Scope to use inside the bucket. If not specified, the _default scope is used.")
      .supportsExpressionLanguage(true)
      .withValidator(core::StandardPropertyTypes::NON_BLANK_VALIDATOR)
      .build();
  EXTENSIONAPI static constexpr auto CollectionName = core::PropertyDefinitionBuilder<>::createProperty("Collection Name")
      .withDescription("Collection to use inside the bucket scope. If not specified, the _default collection is used.")
      .supportsExpressionLanguage(true)
      .withValidator(core::StandardPropertyTypes::NON_BLANK_VALIDATOR)
      .build();
  EXTENSIONAPI static constexpr auto DocumentType = core::PropertyDefinitionBuilder<magic_enum::enum_count<CouchbaseValueType>()>::createProperty("Document Type")
      .withDescription("Content type to store data as.")
      .isRequired(true)
      .withDefaultValue(magic_enum::enum_name(CouchbaseValueType::Json))
      .withAllowedValues(magic_enum::enum_names<CouchbaseValueType>())
      .build();
  EXTENSIONAPI static constexpr auto DocumentId = core::PropertyDefinitionBuilder<>::createProperty("Document Id")
      .withDescription("A static, fixed Couchbase document id, or an expression to construct the Couchbase document id. "
                       "If not specified, either the FlowFile uuid attribute or if that's not found a generated uuid will be used.")
      .supportsExpressionLanguage(true)
      .withValidator(core::StandardPropertyTypes::NON_BLANK_VALIDATOR)
      .build();
  EXTENSIONAPI static constexpr auto PersistTo = core::PropertyDefinitionBuilder<6>::createProperty("Persist To")
      .withDescription("Durability constraint about disk persistence.")
      .isRequired(true)
      .withDefaultValue(magic_enum::enum_name(::couchbase::persist_to::none))
      .withAllowedValues(magic_enum::enum_names<::couchbase::persist_to>())
      .build();
  EXTENSIONAPI static constexpr auto ReplicateTo = core::PropertyDefinitionBuilder<4>::createProperty("Replicate To")
      .withDescription("Durability constraint about replication.")
      .isRequired(true)
      .withDefaultValue(magic_enum::enum_name(::couchbase::replicate_to::none))
      .withAllowedValues(magic_enum::enum_names<::couchbase::replicate_to>())
      .build();

  EXTENSIONAPI static constexpr auto Properties = std::to_array<core::PropertyReference>({
    CouchbaseClusterControllerService,
    BucketName,
    ScopeName,
    CollectionName,
    DocumentType,
    DocumentId,
    PersistTo,
    ReplicateTo
  });

  EXTENSIONAPI static constexpr auto Success = core::RelationshipDefinition{"success", "All FlowFiles that are written to Couchbase Server are routed to this relationship."};
  EXTENSIONAPI static constexpr auto Failure = core::RelationshipDefinition{"failure", "All FlowFiles failed to be written to Couchbase Server and not retry-able are routed to this relationship."};
  EXTENSIONAPI static constexpr auto Retry = core::RelationshipDefinition{"retry", "All FlowFiles failed to be written to Couchbase Server but can be retried are routed to this relationship."};
  EXTENSIONAPI static constexpr auto Relationships = std::array{Success, Failure, Retry};

  EXTENSIONAPI static constexpr auto CouchbaseBucket = core::OutputAttributeDefinition<>{"couchbase.bucket", {Success}, "Bucket where the document was stored."};
  EXTENSIONAPI static constexpr auto CouchbaseDocId = core::OutputAttributeDefinition<>{"couchbase.doc.id", {Success}, "Id of the document."};
  EXTENSIONAPI static constexpr auto CouchbaseDocCas = core::OutputAttributeDefinition<>{"couchbase.doc.cas", {Success}, "CAS of the document."};
  EXTENSIONAPI static constexpr auto CouchbaseDocSequenceNumber = core::OutputAttributeDefinition<>{"couchbase.doc.sequence.number", {Success}, "Sequence number associated with the document."};
  EXTENSIONAPI static constexpr auto CouchbasePartitionUUID = core::OutputAttributeDefinition<>{"couchbase.partition.uuid", {Success}, "UUID of partition."};
  EXTENSIONAPI static constexpr auto CouchbasePartitionId = core::OutputAttributeDefinition<>{"couchbase.partition.id", {Success}, "ID of partition (also known as vBucket)."};
  EXTENSIONAPI static constexpr auto OutputAttributes = std::array<core::OutputAttributeReference, 6>{
      CouchbaseBucket, CouchbaseDocId, CouchbaseDocCas, CouchbaseDocSequenceNumber, CouchbasePartitionUUID, CouchbasePartitionId};

  EXTENSIONAPI static constexpr bool SupportsDynamicProperties = false;
  EXTENSIONAPI static constexpr bool SupportsDynamicRelationships = false;
  EXTENSIONAPI static constexpr core::annotation::Input InputRequirement = core::annotation::Input::INPUT_REQUIRED;
  EXTENSIONAPI static constexpr bool IsSingleThreaded = false;

  void onSchedule(core::ProcessContext& context, core::ProcessSessionFactory& sessionFactory) override;
  void onTrigger(core::ProcessContext& context, core::ProcessSession& session) override;

 private:
  std::shared_ptr<controllers::CouchbaseClusterService> couchbase_cluster_service_;
  CouchbaseValueType document_type_ = CouchbaseValueType::Json;
  ::couchbase::persist_to persist_to_ = ::couchbase::persist_to::none;
  ::couchbase::replicate_to replicate_to_ = ::couchbase::replicate_to::none;
  std::shared_ptr<core::logging::Logger> logger_ = core::logging::LoggerFactory<PutCouchbaseKey>::getLogger(uuid_);
};

}  // namespace org::apache::nifi::minifi::couchbase::processors
