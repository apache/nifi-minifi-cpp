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
#include "FlowFileRecord.h"
#include "core/Processor.h"
#include "core/ProcessSession.h"
#include "core/Core.h"
#include "core/logging/LoggerConfiguration.h"
#include "utils/Export.h"

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
  std::string inputDirectory;
};

class GetFileMetrics : public core::ProcessorMetrics {
 public:
  explicit GetFileMetrics(const core::Processor& source_processor)
    : core::ProcessorMetrics(source_processor) {
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

class GetFile : public core::Processor {
 public:
  explicit GetFile(std::string name, const utils::Identifier& uuid = {})
      : Processor(std::move(name), uuid, std::make_shared<GetFileMetrics>(*this)) {
  }
  ~GetFile() override = default;

  EXTENSIONAPI static constexpr const char* Description = "Creates FlowFiles from files in a directory. MiNiFi will ignore files for which it doesn't have read permissions.";

  EXTENSIONAPI static const core::Property Directory;
  EXTENSIONAPI static const core::Property Recurse;
  EXTENSIONAPI static const core::Property KeepSourceFile;
  EXTENSIONAPI static const core::Property MinAge;
  EXTENSIONAPI static const core::Property MaxAge;
  EXTENSIONAPI static const core::Property MinSize;
  EXTENSIONAPI static const core::Property MaxSize;
  EXTENSIONAPI static const core::Property IgnoreHiddenFile;
  EXTENSIONAPI static const core::Property PollInterval;
  EXTENSIONAPI static const core::Property BatchSize;
  EXTENSIONAPI static const core::Property FileFilter;
  static auto properties() {
    return std::array{
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
    };
  }

  EXTENSIONAPI static const core::Relationship Success;
  static auto relationships() { return std::array{Success}; }

  EXTENSIONAPI static constexpr bool SupportsDynamicProperties = false;
  EXTENSIONAPI static constexpr bool SupportsDynamicRelationships = false;
  EXTENSIONAPI static constexpr core::annotation::Input InputRequirement = core::annotation::Input::INPUT_FORBIDDEN;
  EXTENSIONAPI static constexpr bool IsSingleThreaded = false;

  ADD_COMMON_VIRTUAL_FUNCTIONS_FOR_PROCESSORS

  void onSchedule(core::ProcessContext *context, core::ProcessSessionFactory *sessionFactory) override;
  void onTrigger(core::ProcessContext *context, core::ProcessSession *session) override;
  void initialize() override;

  /**
   * performs a listing on the directory.
   * @param request get file request.
   */
  void performListing(const GetFileRequest &request);

 private:
  bool isListingEmpty() const;
  void putListing(const std::string& fileName);
  std::queue<std::string> pollListing(uint64_t batch_size);
  bool fileMatchesRequestCriteria(const std::string& fullName, const std::string& name, const GetFileRequest &request);
  void getSingleFile(core::ProcessSession& session, const std::string& file_name) const;

  GetFileRequest request_;
  std::queue<std::string> directory_listing_;
  mutable std::mutex directory_listing_mutex_;
  std::atomic<std::chrono::time_point<std::chrono::system_clock>> last_listing_time_{};
  std::shared_ptr<core::logging::Logger> logger_ = core::logging::LoggerFactory<GetFile>::getLogger();
};

}  // namespace org::apache::nifi::minifi::processors
