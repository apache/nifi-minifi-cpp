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
  uint64_t minAge = 0;
  uint64_t maxAge = 0;
  uint64_t minSize = 0;
  uint64_t maxSize = 0;
  bool ignoreHiddenFile = true;
  uint64_t pollInterval = 0;
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

 protected:
  friend class GetFile;

  std::atomic<size_t> iterations_;
  std::atomic<size_t> accepted_files_;
  std::atomic<size_t> input_bytes_;
};

// GetFile Class
class GetFile : public core::Processor, public state::response::MetricsNodeSource {
 public:
  // Constructor
  /*!
   * Create a new processor
   */
  explicit GetFile(const std::string& name, const utils::Identifier& uuid = {})
      : Processor(name, uuid),
        metrics_(std::make_shared<GetFileMetrics>()),
        last_listing_time_(0),
        logger_(logging::LoggerFactory<GetFile>::getLogger()) {
  }
  // Destructor
  ~GetFile() override = default;

  // Processor Name
  EXTENSIONAPI static constexpr char const* ProcessorName = "GetFile";
  // Supported Properties
  EXTENSIONAPI static core::Property Directory;
  EXTENSIONAPI static core::Property Recurse;
  EXTENSIONAPI static core::Property KeepSourceFile;
  EXTENSIONAPI static core::Property MinAge;
  EXTENSIONAPI static core::Property MaxAge;
  EXTENSIONAPI static core::Property MinSize;
  EXTENSIONAPI static core::Property MaxSize;
  EXTENSIONAPI static core::Property IgnoreHiddenFile;
  EXTENSIONAPI static core::Property PollInterval;
  EXTENSIONAPI static core::Property BatchSize;
  EXTENSIONAPI static core::Property FileFilter;
  // Supported Relationships
  EXTENSIONAPI static core::Relationship Success;

 public:
  /**
   * Function that's executed when the processor is scheduled.
   * @param context process context.
   * @param sessionFactory process session factory that is used when creating
   * ProcessSession objects.
   */
  void onSchedule(core::ProcessContext *context, core::ProcessSessionFactory *sessionFactory) override;
  /**
   * Execution trigger for the GetFile Processor
   * @param context processor context
   * @param session processor session reference.
   */
  void onTrigger(core::ProcessContext *context, core::ProcessSession *session) override;

  // Initialize, over write by NiFi GetFile
  void initialize(void) override;
  /**
   * performs a listing on the directory.
   * @param request get file request.
   */
  void performListing(const GetFileRequest &request);

  int16_t getMetricNodes(std::vector<std::shared_ptr<state::response::ResponseNode>> &metric_vector) override;

 private:
  core::annotation::Input getInputRequirement() const override {
    return core::annotation::Input::INPUT_FORBIDDEN;
  }

  bool isListingEmpty() const;
  void putListing(std::string fileName);
  std::queue<std::string> pollListing(uint64_t batch_size);
  bool fileMatchesRequestCriteria(std::string fullName, std::string name, const GetFileRequest &request);
  void getSingleFile(core::ProcessSession& session, const std::string& file_name) const;

  std::shared_ptr<GetFileMetrics> metrics_;
  GetFileRequest request_;
  std::queue<std::string> directory_listing_;
  mutable std::mutex directory_listing_mutex_;
  std::atomic<uint64_t> last_listing_time_;
  std::shared_ptr<logging::Logger> logger_;
};

}  // namespace processors
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org

#endif  // EXTENSIONS_STANDARD_PROCESSORS_PROCESSORS_GETFILE_H_
