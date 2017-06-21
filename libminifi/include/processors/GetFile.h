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
#ifndef __GET_FILE_H__
#define __GET_FILE_H__

#include <atomic>
#include "FlowFileRecord.h"
#include "core/Processor.h"
#include "core/ProcessSession.h"
#include "core/Core.h"
#include "core/Resource.h"
#include "core/logging/LoggerConfiguration.h"
#include "core/state/metrics/MetricsBase.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace processors {

struct GetFileRequest {
  std::string directory = ".";bool recursive = true;bool keepSourceFile = false;
  int64_t minAge = 0;
  int64_t maxAge = 0;
  int64_t minSize = 0;
  int64_t maxSize = 0;
  bool ignoreHiddenFile = true;
  int64_t pollInterval = 0;
  int64_t batchSize = 10;
  std::string fileFilter = "[^\\.].*";
};

class GetFileMetrics : public state::metrics::Metrics {
 public:
  GetFileMetrics()
      : state::metrics::Metrics("GetFileMetrics", 0) {
    iterations_ = 0;
    accepted_files_ = 0;
    input_bytes_ = 0;
  }

  GetFileMetrics(std::string name, uuid_t uuid)
      : state::metrics::Metrics(name, uuid)
  {
    iterations_ = 0;
    accepted_files_ = 0;
    input_bytes_ = 0;
  }
  virtual ~GetFileMetrics() {

  }
  virtual std::string getName() {
    return core::Connectable::getName();
  }

  virtual std::vector<state::metrics::MetricResponse> serialize() {
    std::vector<state::metrics::MetricResponse> resp;


    state::metrics::MetricResponse iter;
    iter.name = "OnTriggerInvocations";
    iter.value = std::to_string(iterations_.load());

    resp.push_back(iter);

    state::metrics::MetricResponse accepted_files;
    accepted_files.name = "AcceptedFiles";
    accepted_files.value = std::to_string(accepted_files_.load());

    resp.push_back(accepted_files);

    state::metrics::MetricResponse input_bytes;
    input_bytes.name = "InputBytes";
    input_bytes.value = std::to_string(input_bytes_.load());

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
class GetFile : public core::Processor, public state::metrics::MetricsSource {
 public:
  // Constructor
  /*!
   * Create a new processor
   */
  explicit GetFile(std::string name, uuid_t uuid = NULL)
      : Processor(name, uuid),
        logger_(logging::LoggerFactory<GetFile>::getLogger()) {
    metrics_ = std::make_shared<GetFileMetrics>();
  }
  // Destructor
  virtual ~GetFile() {
  }
  // Processor Name
  static constexpr char const* ProcessorName = "GetFile";
  // Supported Properties
  static core::Property Directory;
  static core::Property Recurse;
  static core::Property KeepSourceFile;
  static core::Property MinAge;
  static core::Property MaxAge;
  static core::Property MinSize;
  static core::Property MaxSize;
  static core::Property IgnoreHiddenFile;
  static core::Property PollInterval;
  static core::Property BatchSize;
  static core::Property FileFilter;
  // Supported Relationships
  static core::Relationship Success;

 public:
  /**
   * Function that's executed when the processor is scheduled.
   * @param context process context.
   * @param sessionFactory process session factory that is used when creating
   * ProcessSession objects.
   */
  void onSchedule(core::ProcessContext *context, core::ProcessSessionFactory *sessionFactory);
  /**
   * Execution trigger for the GetFile Processor
   * @param context processor context
   * @param session processor session reference.
   */
  virtual void onTrigger(core::ProcessContext *context, core::ProcessSession *session);

  // Initialize, over write by NiFi GetFile
  virtual void initialize(void);
  /**
   * performs a listeing on the directory.
   * @param dir directory to list
   * @param request get file request.
   */
  void performListing(std::string dir, const GetFileRequest &request);

  int16_t getMetrics(std::vector<std::shared_ptr<state::metrics::Metrics>> &metric_vector);

 protected:

 private:

  std::shared_ptr<GetFileMetrics> metrics_;

  // Queue for store directory list
  std::queue<std::string> _dirList;
  // Get Listing size
  uint64_t getListingSize() {
    std::lock_guard<std::mutex> lock(mutex_);
    return _dirList.size();
  }
  // Whether the directory listing is empty
  bool isListingEmpty();
  // Put full path file name into directory listing
  void putListing(std::string fileName);
  // Poll directory listing for files
  void pollListing(std::queue<std::string> &list, const GetFileRequest &request);
  // Check whether file can be added to the directory listing
  bool acceptFile(std::string fullName, std::string name, const GetFileRequest &request);
  // Get file request object.
  GetFileRequest request_;
  // Mutex for protection of the directory listing

  std::mutex mutex_;

  // last listing time for root directory ( if recursive, we will consider the root
  // as the top level time.
  std::atomic<uint64_t> last_listing_time_;

  std::shared_ptr<logging::Logger> logger_;
};

REGISTER_RESOURCE(GetFile);

} /* namespace processors */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

#endif
