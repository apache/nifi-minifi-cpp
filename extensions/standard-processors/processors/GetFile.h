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
#ifndef EXTENSIONS_STANDARD_PROCESSORS_PROCESSORS_GETFILE_H_
#define EXTENSIONS_STANDARD_PROCESSORS_PROCESSORS_GETFILE_H_

#include <memory>
#include <queue>
#include <string>
#include <vector>
#include <atomic>

#include "core/state/nodes/MetricsBase.h"
#include "FlowFileRecord.h"
#include "core/Processor.h"
#include "core/ProcessSession.h"
#include "core/Core.h"
#include "core/logging/LoggerConfiguration.h"
#include "utils/Export.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace processors {

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
  std::string fileFilter = "[^\\.].*";
  std::string inputDirectory;
};

class GetFileMetrics : public state::response::ResponseNode {
 public:
  GetFileMetrics()
      : state::response::ResponseNode("GetFileMetrics") {
    iterations_ = 0;
    accepted_files_ = 0;
    input_bytes_ = 0;
  }

  GetFileMetrics(const std::string& name, const utils::Identifier& uuid)
      : state::response::ResponseNode(name, uuid) {
    iterations_ = 0;
    accepted_files_ = 0;
    input_bytes_ = 0;
  }
  virtual ~GetFileMetrics() = default;
  std::string getName() const override {
    return core::Connectable::getName();
  }

  std::vector<state::response::SerializedResponseNode> serialize() override {
    std::vector<state::response::SerializedResponseNode> resp;

    state::response::SerializedResponseNode iter;
    iter.name = "OnTriggerInvocations";
    iter.value = (uint32_t)iterations_.load();

    resp.push_back(iter);

    state::response::SerializedResponseNode accepted_files;
    accepted_files.name = "AcceptedFiles";
    accepted_files.value = (uint32_t)accepted_files_.load();

    resp.push_back(accepted_files);

    state::response::SerializedResponseNode input_bytes;
    input_bytes.name = "InputBytes";
    input_bytes.value = (uint32_t)input_bytes_.load();

    resp.push_back(input_bytes);

    return resp;
  }

  std::vector<state::PublishedMetric> calculateMetrics() override {
    return {
      {"onTrigger_invocations", static_cast<double>(iterations_.load()), {{"metric_class", getName()}}},
      {"accepted_files", static_cast<double>(accepted_files_.load()), {{"metric_class", getName()}}},
      {"input_bytes", static_cast<double>(input_bytes_.load()), {{"metric_class", getName()}}}
    };
  }

 protected:
  friend class GetFile;

  std::atomic<size_t> iterations_;
  std::atomic<size_t> accepted_files_;
  std::atomic<size_t> input_bytes_;
};

class GetFile : public core::Processor, public state::response::MetricsNodeSource {
 public:
  explicit GetFile(const std::string& name, const utils::Identifier& uuid = {})
      : Processor(name, uuid),
        metrics_(std::make_shared<GetFileMetrics>()) {
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

  int16_t getMetricNodes(std::vector<std::shared_ptr<state::response::ResponseNode>> &metric_vector) override;

 private:
  bool isListingEmpty() const;
  void putListing(std::string fileName);
  std::queue<std::string> pollListing(uint64_t batch_size);
  bool fileMatchesRequestCriteria(std::string fullName, std::string name, const GetFileRequest &request);
  void getSingleFile(core::ProcessSession& session, const std::string& file_name) const;

  std::shared_ptr<GetFileMetrics> metrics_;
  GetFileRequest request_;
  std::queue<std::string> directory_listing_;
  mutable std::mutex directory_listing_mutex_;
  std::atomic<std::chrono::time_point<std::chrono::system_clock>> last_listing_time_{};
  std::shared_ptr<core::logging::Logger> logger_ = core::logging::LoggerFactory<GetFile>::getLogger();
};

}  // namespace processors
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org

#endif  // EXTENSIONS_STANDARD_PROCESSORS_PROCESSORS_GETFILE_H_
