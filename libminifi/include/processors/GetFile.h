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
#include "core/core.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace processors {

  struct GetFileRequest{
    std::string directory = ".";
    bool recursive = true;
    bool keepSourceFile = false;
    int64_t minAge = 0;
    int64_t maxAge = 0;
    int64_t minSize = 0;
    int64_t maxSize = 0;
    bool ignoreHiddenFile = true;
    int64_t pollInterval = 0;
    int64_t batchSize = 10;
    std::string fileFilter= "[^\\.].*";
  };

// GetFile Class
class GetFile : public core::Processor {
 public:
  // Constructor
  /*!
   * Create a new processor
   */
  explicit GetFile(std::string name, uuid_t uuid = NULL)
      : Processor(name, uuid) {

  }
  // Destructor
  virtual ~GetFile() {
  }
  // Processor Name
  static const std::string ProcessorName;
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
  // OnTrigger method, implemented by NiFi GetFile
  virtual void onTrigger(
      core::ProcessContext *context,
      core::ProcessSession *session);
  /**
   * Function that's executed when the processor is scheduled.
   * @param context process context.
   * @param sessionFactory process session factory that is used when creating
   * ProcessSession objects.
   */
  void onSchedule(
        core::ProcessContext *context,
        core::ProcessSessionFactory *sessionFactory);
  // Initialize, over write by NiFi GetFile
  virtual void initialize(void);
  // perform directory listing
  void performListing(std::string dir,const GetFileRequest &request);

  uint64_t getLastListingTime(const std::string &directory);

  void updateListingTime(const std::string &directory);
 protected:

 private:

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
  void pollListing(std::queue<std::string> &list,const GetFileRequest &request);
  // Check whether file can be added to the directory listing
  bool acceptFile(std::string fullName, std::string name, const GetFileRequest &request);
  // Get file request object.
  GetFileRequest request_;
  // Mutex for protection of the directory listing

  std::mutex mutex_;

  std::map<std::string,std::atomic<uint64_t>> last_listing_times_;

};

} /* namespace processors */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

#endif
