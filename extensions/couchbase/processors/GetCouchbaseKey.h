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
#include "core/logging/LoggerFactory.h"
#include "CouchbaseClusterService.h"

namespace org::apache::nifi::minifi::couchbase::processors {

class GetCouchbaseKey final : public core::AbstractProcessor<GetCouchbaseKey> {
 public:
  using core::AbstractProcessor<GetCouchbaseKey>::AbstractProcessor;

  EXTENSIONAPI static constexpr const char* Description = "Get a document from Couchbase Server via Key/Value access. The ID of the document to fetch may be supplied by setting the "
    "<Document Id> property. NOTE: if the Document Id property is not set, the contents of the FlowFile will be read to determine the Document Id, which means that the contents of "
    "the entire FlowFile will be buffered in memory.";

  EXTENSIONAPI static constexpr auto CouchbaseClusterControllerService = core::PropertyDefinitionBuilder<>::createProperty("Couchbase Cluster Controller Service")
      .withDescription("A Couchbase Cluster Controller Service which manages connections to a Couchbase cluster.")
      .withAllowedTypes<controllers::CouchbaseClusterService>()
      .isRequired(true)
      .build();
  EXTENSIONAPI static constexpr auto BucketName = core::PropertyDefinitionBuilder<>::createProperty("Bucket Name")
      .withDescription("The name of bucket to access.")
      .withDefaultValue("default")
      .isRequired(true)
      .withValidator(core::StandardPropertyValidators::NON_BLANK_VALIDATOR)
      .supportsExpressionLanguage(true)
      .build();
  EXTENSIONAPI static constexpr auto ScopeName = core::PropertyDefinitionBuilder<>::createProperty("Scope Name")
      .withDescription("Scope to use inside the bucket. If not specified, the _default scope is used.")
      .supportsExpressionLanguage(true)
      .withValidator(core::StandardPropertyValidators::NON_BLANK_VALIDATOR)
      .build();
  EXTENSIONAPI static constexpr auto CollectionName = core::PropertyDefinitionBuilder<>::createProperty("Collection Name")
      .withDescription("Collection to use inside the bucket scope. If not specified, the _default collection is used.")
      .supportsExpressionLanguage(true)
      .withValidator(core::StandardPropertyValidators::NON_BLANK_VALIDATOR)
      .build();
  EXTENSIONAPI static constexpr auto DocumentType = core::PropertyDefinitionBuilder<3>::createProperty("Document Type")
      .withDescription("Content type of the retrieved value.")
      .isRequired(true)
      .withDefaultValue(magic_enum::enum_name(CouchbaseValueType::Json))
      .withAllowedValues(magic_enum::enum_names<CouchbaseValueType>())
      .build();
  EXTENSIONAPI static constexpr auto DocumentId = core::PropertyDefinitionBuilder<>::createProperty("Document Id")
      .withDescription("A static, fixed Couchbase document id, or an expression to construct the Couchbase document id.")
      .supportsExpressionLanguage(true)
      .withValidator(core::StandardPropertyValidators::NON_BLANK_VALIDATOR)
      .build();
  EXTENSIONAPI static constexpr auto PutValueToAttribute = core::PropertyDefinitionBuilder<>::createProperty("Put Value to Attribute")
      .withDescription("If set, the retrieved value will be put into an attribute of the FlowFile instead of a the content of the FlowFile. "
                       "The attribute key to put to is determined by evaluating value of this property.")
      .supportsExpressionLanguage(true)
      .build();

  EXTENSIONAPI static constexpr auto Properties = std::to_array<core::PropertyReference>({
    CouchbaseClusterControllerService,
    BucketName,
    ScopeName,
    CollectionName,
    DocumentType,
    DocumentId,
    PutValueToAttribute
  });

  EXTENSIONAPI static constexpr auto Success = core::RelationshipDefinition{"success",
      "Values retrieved from Couchbase Server are written as outgoing FlowFiles content or put into an attribute of the incoming FlowFile and routed to this relationship."};
  EXTENSIONAPI static constexpr auto Failure = core::RelationshipDefinition{"failure", "All FlowFiles failed to fetch from Couchbase Server and not retry-able are routed to this relationship."};
  EXTENSIONAPI static constexpr auto Retry = core::RelationshipDefinition{"retry", "All FlowFiles failed to fetch from Couchbase Server but can be retried are routed to this relationship."};
  EXTENSIONAPI static constexpr auto Original = core::RelationshipDefinition{"original",
      "The original input FlowFile is routed to this relationship when the value is retrieved from Couchbase Server and routed to 'success'."};
  EXTENSIONAPI static constexpr auto Relationships = std::array{Success, Failure, Retry, Original};

  EXTENSIONAPI static constexpr auto CouchbaseBucket = core::OutputAttributeDefinition<>{"couchbase.bucket", {Success}, "Bucket where the document was stored."};
  EXTENSIONAPI static constexpr auto CouchbaseDocId = core::OutputAttributeDefinition<>{"couchbase.doc.id", {Success}, "Id of the document."};
  EXTENSIONAPI static constexpr auto CouchbaseDocCas = core::OutputAttributeDefinition<>{"couchbase.doc.cas", {Success}, "CAS of the document."};
  EXTENSIONAPI static constexpr auto CouchbaseDocExpiry = core::OutputAttributeDefinition<>{"couchbase.doc.expiry", {Success}, "Expiration of the document."};
  EXTENSIONAPI static constexpr auto OutputAttributes = std::array<core::OutputAttributeReference, 4>{
      CouchbaseBucket, CouchbaseDocId, CouchbaseDocCas, CouchbaseDocExpiry};

  EXTENSIONAPI static constexpr bool SupportsDynamicProperties = false;
  EXTENSIONAPI static constexpr bool SupportsDynamicRelationships = false;
  EXTENSIONAPI static constexpr core::annotation::Input InputRequirement = core::annotation::Input::INPUT_REQUIRED;
  EXTENSIONAPI static constexpr bool IsSingleThreaded = false;

  void onSchedule(core::ProcessContext& context, core::ProcessSessionFactory& sessionFactory) override;
  void onTrigger(core::ProcessContext& context, core::ProcessSession& session) override;

 private:
  std::shared_ptr<controllers::CouchbaseClusterService> couchbase_cluster_service_;
  CouchbaseValueType document_type_ = CouchbaseValueType::Json;
  std::shared_ptr<core::logging::Logger> logger_ = core::logging::LoggerFactory<GetCouchbaseKey>::getLogger(uuid_);
};

}  // namespace org::apache::nifi::minifi::couchbase::processors
