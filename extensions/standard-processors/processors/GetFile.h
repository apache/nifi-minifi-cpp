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
#include <queue>
#include <string>
#include <vector>
#include <atomic>
#include <utility>

#include "core/state/nodes/MetricsBase.h"
#include "core/Processor.h"
#include "core/ProcessSession.h"
#include "core/PropertyDefinition.h"
#include "core/PropertyDefinitionBuilder.h"
#include "core/PropertyType.h"
#include "core/RelationshipDefinition.h"
#include "core/Core.h"
#include "core/logging/LoggerFactory.h"
#include "utils/Export.h"
#include "core/ProcessorMetrics.h"

namespace org::apache::nifi::minifi::processors {

struct GetFileRequest {
  bool recursive = true;
  bool keepSourceFile = false;
  std::chrono::milliseconds minAge{0};
  std::chrono::milliseconds maxAge{0};
  uint64_t minSize = 0;
  uint64_t maxSize = 0;
  bool ignoreHiddenFile = true;
  std::chrono::milliseconds pollInterval{0};
  uint64_t batchSize = 10;
  std::string fileFilter = ".*";
  std::filesystem::path inputDirectory;
};

class GetFileMetrics : public core::ProcessorMetricsImpl {
 public:
  explicit GetFileMetrics(const core::Processor& source_processor)
    : core::ProcessorMetricsImpl(source_processor) {
  }

  std::vector<state::response::SerializedResponseNode> serialize() override {
    auto resp = core::ProcessorMetrics::serialize();
    auto& root_node = resp[0];

    state::response::SerializedResponseNode accepted_files_node{"AcceptedFiles", accepted_files.load()};
    root_node.children.push_back(accepted_files_node);

    state::response::SerializedResponseNode input_bytes_node{"InputBytes", input_bytes.load()};
    root_node.children.push_back(input_bytes_node);

    return resp;
  }

  std::vector<state::PublishedMetric> calculateMetrics() override {
    auto metrics = core::ProcessorMetrics::calculateMetrics();
    metrics.push_back({"accepted_files", static_cast<double>(accepted_files.load()), getCommonLabels()});
    metrics.push_back({"input_bytes", static_cast<double>(input_bytes.load()), getCommonLabels()});
    return metrics;
  }

  std::atomic<uint32_t> accepted_files{0};
  std::atomic<uint64_t> input_bytes{0};
};

class GetFile : public core::ProcessorImpl {
 public:
  explicit GetFile(std::string_view name, const utils::Identifier& uuid = {})
      : ProcessorImpl(name, uuid, std::make_shared<GetFileMetrics>(*this)) {
  }
  ~GetFile() override = default;

  EXTENSIONAPI static constexpr const char* Description = "Creates FlowFiles from files in a directory. MiNiFi will ignore files for which it doesn't have read permissions.";

  EXTENSIONAPI static constexpr auto Directory = core::PropertyDefinitionBuilder<>::createProperty("Input Directory")
      .withDescription("The input directory from which to pull files")
      .isRequired(true)
      .supportsExpressionLanguage(true)
      .build();
  EXTENSIONAPI static constexpr auto Recurse = core::PropertyDefinitionBuilder<>::createProperty("Recurse Subdirectories")
      .withDescription("Indicates whether or not to pull files from subdirectories")
      .withPropertyType(core::StandardPropertyTypes::BOOLEAN_TYPE)
      .withDefaultValue("true")
      .build();
  EXTENSIONAPI static constexpr auto KeepSourceFile = core::PropertyDefinitionBuilder<>::createProperty("Keep Source File")
      .withDescription("If true, the file is not deleted after it has been copied to the Content Repository")
      .withPropertyType(core::StandardPropertyTypes::BOOLEAN_TYPE)
      .withDefaultValue("false")
      .build();
  EXTENSIONAPI static constexpr auto MinAge = core::PropertyDefinitionBuilder<>::createProperty("Minimum File Age")
      .withDescription("The minimum age that a file must be in order to be pulled;"
          " any file younger than this amount of time (according to last modification date) will be ignored")
      .withPropertyType(core::StandardPropertyTypes::TIME_PERIOD_TYPE)
      .withDefaultValue("0 sec")
      .build();
  EXTENSIONAPI static constexpr auto MaxAge = core::PropertyDefinitionBuilder<>::createProperty("Maximum File Age")
      .withDescription("The maximum age that a file must be in order to be pulled;"
          " any file older than this amount of time (according to last modification date) will be ignored")
      .withPropertyType(core::StandardPropertyTypes::TIME_PERIOD_TYPE)
      .withDefaultValue("0 sec")
      .build();
  EXTENSIONAPI static constexpr auto MinSize = core::PropertyDefinitionBuilder<>::createProperty("Minimum File Size")
      .withDescription("The minimum size that a file can be in order to be pulled")
      .withPropertyType(core::StandardPropertyTypes::DATA_SIZE_TYPE)
      .withDefaultValue("0 B")
      .build();
  EXTENSIONAPI static constexpr auto MaxSize = core::PropertyDefinitionBuilder<>::createProperty("Maximum File Size")
      .withDescription("The maximum size that a file can be in order to be pulled")
      .withPropertyType(core::StandardPropertyTypes::DATA_SIZE_TYPE)
      .withDefaultValue("0 B")
      .build();
  EXTENSIONAPI static constexpr auto IgnoreHiddenFile = core::PropertyDefinitionBuilder<>::createProperty("Ignore Hidden Files")
      .withDescription("Indicates whether or not hidden files should be ignored")
      .withPropertyType(core::StandardPropertyTypes::BOOLEAN_TYPE)
      .withDefaultValue("true")
      .build();
  EXTENSIONAPI static constexpr auto PollInterval = core::PropertyDefinitionBuilder<>::createProperty("Polling Interval")
      .withDescription("Indicates how long to wait before performing a directory listing")
      .withPropertyType(core::StandardPropertyTypes::TIME_PERIOD_TYPE)
      .withDefaultValue("0 sec")
      .build();
  EXTENSIONAPI static constexpr auto BatchSize = core::PropertyDefinitionBuilder<>::createProperty("Batch Size")
      .withDescription("The maximum number of files to pull in each iteration")
      .withPropertyType(core::StandardPropertyTypes::UNSIGNED_INT_TYPE)
      .withDefaultValue("10")
      .build();
  EXTENSIONAPI static constexpr auto FileFilter = core::PropertyDefinitionBuilder<>::createProperty("File Filter")
      .withDescription("Only files whose names match the given regular expression will be picked up")
      .withDefaultValue(".*")
      .build();
  EXTENSIONAPI static constexpr auto Properties = std::to_array<core::PropertyReference>({
      Directory,
      Recurse,
      KeepSourceFile,
      MinAge,
      MaxAge,
      MinSize,
      MaxSize,
      IgnoreHiddenFile,
      PollInterval,
      BatchSize,
      FileFilter
  });


  EXTENSIONAPI static constexpr auto Success = core::RelationshipDefinition{"success", "All files are routed to success"};
  EXTENSIONAPI static constexpr auto Relationships = std::array{Success};

  EXTENSIONAPI static constexpr bool SupportsDynamicProperties = false;
  EXTENSIONAPI static constexpr bool SupportsDynamicRelationships = false;
  EXTENSIONAPI static constexpr core::annotation::Input InputRequirement = core::annotation::Input::INPUT_FORBIDDEN;
  EXTENSIONAPI static constexpr bool IsSingleThreaded = false;

  ADD_COMMON_VIRTUAL_FUNCTIONS_FOR_PROCESSORS

  void onSchedule(core::ProcessContext& context, core::ProcessSessionFactory& session_factory) override;
  void onTrigger(core::ProcessContext& context, core::ProcessSession& session) override;
  void initialize() override;

  /**
   * performs a listing on the directory.
   * @param request get file request.
   */
  void performListing(const GetFileRequest &request);

 private:
  bool isListingEmpty() const;
  void putListing(const std::filesystem::path& file_path);
  std::queue<std::filesystem::path> pollListing(uint64_t batch_size);
  bool fileMatchesRequestCriteria(const std::filesystem::path& full_name, const std::filesystem::path& name, const GetFileRequest &request);
  void getSingleFile(core::ProcessSession& session, const std::filesystem::path& file_path) const;

  GetFileRequest request_;
  std::queue<std::filesystem::path> directory_listing_;
  mutable std::mutex directory_listing_mutex_;
  std::atomic<std::chrono::time_point<std::chrono::system_clock>> last_listing_time_{};
  std::shared_ptr<core::logging::Logger> logger_ = core::logging::LoggerFactory<GetFile>::getLogger(uuid_);
};

}  // namespace org::apache::nifi::minifi::processors
