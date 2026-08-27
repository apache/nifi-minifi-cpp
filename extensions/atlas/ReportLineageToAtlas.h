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
#pragma once

#include <array>
#include <cstddef>
#include <memory>
#include <string>

#include "ExtractorDispatcher.h"
#include "NamespaceResolver.h"
#include "core/PropertyDefinitionBuilder.h"
#include "core/reporting/ReportingTaskBase.h"
#include "minifi-cpp/controllers/SSLContextServiceInterface.h"

namespace org::apache::nifi::minifi::extensions::atlas {

// ReportLineageToAtlas reports the MiNiFi agent's flow topology and per‑event lineage
// edges to Apache Atlas via the REST v2 API. Modeled on NiFi's ReportLineageToAtlas,
// scoped down: REST v2 only (no Kafka hook bus), SimplePath strategy only, Basic auth.
class ReportLineageToAtlas : public core::reporting::ReportingTaskBase {
 public:
  explicit ReportLineageToAtlas(core::reporting::ReportingTaskMetadata metadata)
      : ReportingTaskBase{std::move(metadata)} {}

  MINIFIAPI static constexpr const char* Description = "Report flow data set level lineage to Apache Atlas.";

  MINIFIAPI static constexpr auto AtlasUrls =
      core::PropertyDefinitionBuilder<>::createProperty("Atlas URLs")
          .withDescription("Comma-separated list of Atlas REST endpoints, e.g. http://atlas:21000")
          .isRequired(true)
          .build();

  MINIFIAPI static constexpr auto AtlasUsername =
      core::PropertyDefinitionBuilder<>::createProperty("Atlas Username")
          .withDescription("Username for Atlas HTTP Basic authentication.")
          .isRequired(false)
          .build();

  MINIFIAPI static constexpr auto AtlasPassword =
      core::PropertyDefinitionBuilder<>::createProperty("Atlas Password")
          .withDescription("Password for Atlas HTTP Basic authentication.")
          .isSensitive(true)
          .isRequired(false)
          .build();

  MINIFIAPI static constexpr auto SSLContext =
      core::PropertyDefinitionBuilder<>::createProperty("SSL Context Service")
          .withDescription("Controller service used for HTTPS Atlas endpoints.")
          .isRequired(false)
          .build();

  MINIFIAPI static constexpr auto NiFiUrl =
      core::PropertyDefinitionBuilder<>::createProperty("NiFi URL for Atlas")
          .withDescription("How this MiNiFi agent identifies itself to Atlas (used as nifi_flow.url).")
          .isRequired(true)
          .build();

  MINIFIAPI static constexpr auto DefaultNamespace =
      core::PropertyDefinitionBuilder<>::createProperty("Default Metadata Namespace")
          .withDescription("Atlas metadata namespace used when no hostnamePattern.* rule matches. "
                           "This is the '@cluster' suffix in every qualifiedName.")
          .isRequired(true)
          .build();

  MINIFIAPI static constexpr auto AwsS3ModelVersion =
      core::PropertyDefinitionBuilder<2>::createProperty("AWS S3 Model Version")
          .withDescription("Which Atlas S3 entity model to emit. v2 uses aws_s3_v2_directory; v1 uses aws_s3_pseudo_dir.")
          .isRequired(true)
          .withDefaultValue("v2")
          .withAllowedValues({"v1", "v2"})
          .build();

  MINIFIAPI static constexpr auto FilesystemPathLevel =
      core::PropertyDefinitionBuilder<2>::createProperty("Filesystem Path Level")
          .withDescription("Whether to emit one fs_path entity per file, or per containing directory.")
          .isRequired(true)
          .withDefaultValue("DIRECTORY")
          .withAllowedValues({"FILE", "DIRECTORY"})
          .build();

  MINIFIAPI static constexpr auto ProvenanceBatchSize =
      core::PropertyDefinitionBuilder<>::createProperty("Provenance Batch Size")
          .withDescription("Maximum number of provenance events to consume per trigger.")
          .isRequired(true)
          .withDefaultValue("1000")
          .build();

  MINIFIAPI static constexpr auto Properties = std::to_array<core::PropertyReference>({
      AtlasUrls,
      AtlasUsername,
      AtlasPassword,
      SSLContext,
      NiFiUrl,
      DefaultNamespace,
      AwsS3ModelVersion,
      FilesystemPathLevel,
      ProvenanceBatchSize,
  });

  MINIFIAPI static constexpr bool SupportsDynamicProperties = false;

  void initialize() override;
  void onSchedule(core::reporting::ReportingTaskContext& context) override;
  void onTrigger(core::reporting::ReportingTaskContext& context) override;

  enum class S3ModelVersion { V1, V2 };
  enum class FsPathLevel { FILE, DIRECTORY };

 private:
  // Populated in onSchedule from properties.
  std::vector<std::string> atlas_base_urls_;
  std::optional<std::string> atlas_username_;
  std::optional<std::string> atlas_password_;
  std::shared_ptr<controllers::SSLContextServiceInterface> ssl_context_service_;
  std::string nifi_url_;
  NamespaceResolver namespace_resolver_;
  S3ModelVersion s3_model_version_ = S3ModelVersion::V2;
  FsPathLevel fs_path_level_ = FsPathLevel::DIRECTORY;
  std::size_t provenance_batch_size_ = 1000;

  // Persistent across triggers.
  ExtractorDispatcher extractor_dispatcher_;
  bool type_defs_registered_ = false;
};

}  // namespace org::apache::nifi::minifi::extensions::atlas
